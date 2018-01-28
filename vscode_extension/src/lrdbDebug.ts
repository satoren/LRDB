import {
	DebugSession,
	InitializedEvent, TerminatedEvent, ContinuedEvent, StoppedEvent, BreakpointEvent, OutputEvent, Event,
	Thread, StackFrame, Scope, Source, Handles, Breakpoint
} from 'vscode-debugadapter';
import { DebugProtocol } from 'vscode-debugprotocol';
import { readFileSync } from 'fs';
import { fork, spawn, ChildProcess } from 'child_process';
import * as net from 'net';
import * as path from 'path';

import * as os from 'os';
import * as stream from 'stream';


export interface LaunchRequestArguments extends DebugProtocol.LaunchRequestArguments {

	program: string;
	args: string[];
	cwd: string;

	useInternalLua?: boolean;
	port: number;
	sourceRoot?: string;
	stopOnEntry?: boolean;
}


export interface AttachRequestArguments extends DebugProtocol.AttachRequestArguments {
	host: string;
	port: number;
	sourceRoot: string;

	stopOnEntry?: boolean;
}




export interface DebugServerEvent {
	method: string;
	params: any;
	param?: any;//
	error?: any;
	id: any;
}


class VariableReference {
	public constructor(public msg_name: string, public msg_param: any, public datapath: string[]) {
	}
}


interface LRDBClient {
	send(method: string, param?: any, callback?: (response: any) => void);
	end();
	on_event: (event: DebugServerEvent) => void;
	on_close: () => void;
	on_open: () => void;
	on_error: (e: any) => void;
	on_data: (data: string) => void;
}

class LRDBTCPClient {
	private _connection: net.Socket;

	private _callback_map = {};
	private _request_id = 0;
	private _end = false;

	private on_close_() {
		for (var key in this._callback_map) {
			this._callback_map[key]({ result: null, id: key });
			delete this._callback_map[key];
		}

		if (this.on_close) {
			this.on_close();
		}
	}
	private on_connect_() {
		this._connection.on('close', () => {
			this.on_close_();
		});
		if (this.on_open) {
			this.on_open();
		}
	}

	public constructor(port: number, host: string) {
		this._connection = net.connect(port, host);
		this._connection.on('connect', () => {
			this.on_connect_();
		});

		var retryCount = 0;
		this._connection.on('error', (e: any) => {
			if (e.code == 'ECONNREFUSED' && retryCount < 5) {
				retryCount++;
				this._connection.setTimeout(1000, () => {
					if (!this._end) {
						this._connection.connect(port, host);
					}
				});
				return;
			}

			console.error(e.message);
			if (this.on_error) {
				this.on_error(e);
			}
		});

		var chunk = "";
		var ondata = (data) => {
			chunk += data.toString();
			var d_index = chunk.indexOf('\n');
			while (d_index > -1) {
				try {
					var string = chunk.substring(0, d_index);
					var json = JSON.parse(string);
					this.receive(json);
				}
				finally { }
				chunk = chunk.substring(d_index + 1);
				d_index = chunk.indexOf('\n');
			}
		}
		this._connection.on('data', ondata);
	}

	public send(method: string, param?: any, callback?: (response: any) => void) {
		let id = this._request_id++;
		//TODO need remove param
		var data = JSON.stringify({ "jsonrpc": "2.0", "method": method, "params": param, "param": param, "id": id }) + "\n";
		var ret = this._connection.write(data);

		if (callback) {
			if (ret) {
				this._callback_map[id] = callback
			}
			else {
				setTimeout(function () {
					callback({ result: null, id: id });
				}, 0);
			}
		}
	}
	public receive(event: DebugServerEvent) {
		if (this._callback_map[event.id]) {
			this._callback_map[event.id](event);
			delete this._callback_map[event.id];
		}
		else {
			if (this.on_event) {
				this.on_event(event);
			}
		}
	}
	public end() {
		this._end = true;
		this._connection.end();
	}

	on_event: (event: DebugServerEvent) => void;
	on_data: (data: string) => void;
	on_close: () => void;
	on_open: () => void;
	on_error: (e: any) => void;
}


class LRDBChildProcessClient {
	private _callback_map = {};
	private _request_id = 0;

	public constructor(private _child: ChildProcess) {

		setTimeout(() => {
			if (this.on_open) {
				this.on_open();
			}
		}, 0);
		_child.on("message", (msg) => {
			this.receive(msg);
		});
	}


	public send(method: string, param?: any, callback?: (response: any) => void) {
		let id = this._request_id++;

		var ret = this._child.send({ "jsonrpc": "2.0", "method": method, "params": param, "id": id });

		if (callback) {
			if (ret) {
				this._callback_map[id] = callback
			}
			else {
				setTimeout(function () {
					callback({ result: null, id: id });
				}, 0);
			}
		}
	}
	public receive(event: DebugServerEvent) {
		if (event.params == null) {
			event.params = event.param;
		}
		if (this._callback_map[event.id]) {
			this._callback_map[event.id](event);
			delete this._callback_map[event.id];
		}
		else {
			if (this.on_event) {
				this.on_event(event);
			}
		}
	}
	public end() {
		for (var key in this._callback_map) {
			this._callback_map[key]({ result: null, id: key });
			delete this._callback_map[key];
		}
	}
	on_event: (event: DebugServerEvent) => void;
	on_data: (data: string) => void;
	on_close: () => void;
	on_open: () => void;
	on_error: (e: any) => void;
}


class LuaDebugSession extends DebugSession {

	// Lua 
	private static THREAD_ID = 1;

	private _debug_server_process: ChildProcess;

	private _debug_client: LRDBClient;


	// maps from sourceFile to array of Breakpoints
	private _breakPoints = new Map<string, DebugProtocol.Breakpoint[]>();

	private _breakPointID = 1000;

	private _variableHandles = new Handles<VariableReference>();


	private _sourceHandles = new Handles<string>();

	private _stopOnEntry: boolean;


	/**
	 * Creates a new debug adapter that is used for one debug session.
	 * We configure the default implementation of a debug adapter here.
	 */
	public constructor() {
		super();

		// this debugger uses zero-based lines and columns
		this.setDebuggerLinesStartAt1(false);
		this.setDebuggerColumnsStartAt1(false);
	}

	/**
	 * The 'initialize' request is the first request called by the frontend
	 * to interrogate the features the debug adapter provides.
	 */
	protected initializeRequest(response: DebugProtocol.InitializeResponse, args: DebugProtocol.InitializeRequestArguments): void {
		if (this._debug_server_process) {
			this._debug_server_process.kill();
			delete this._debug_server_process;
		}
		if (this._debug_client) {
			this._debug_client.end();
			delete this._debug_client;
		}
		// This debug adapter implements the configurationDoneRequest.
		response.body.supportsConfigurationDoneRequest = true;

		response.body.supportsConditionalBreakpoints = true;

		response.body.supportsHitConditionalBreakpoints = true;

		// make VS Code to use 'evaluate' when hovering over source
		response.body.supportsEvaluateForHovers = true;

		this.sendResponse(response);
	}

	private setupSourceEnv(sourceRoot: string) {
		this.convertClientLineToDebugger = (line: number): number => {
			return line;
		}
		this.convertDebuggerLineToClient = (line: number): number => {
			return line;
		}

		this.convertClientPathToDebugger = (clientPath: string): string => {
			return path.relative(sourceRoot, clientPath);
		}
		this.convertDebuggerPathToClient = (debuggerPath: string): string => {
			if (!debuggerPath.startsWith("@")) { return ''; }
			const filename: string = debuggerPath.substr(1);
			if (path.isAbsolute(filename)) {
				return filename;
			}
			else {
				return path.join(sourceRoot, filename);
			}
		}
	}

	protected launchRequest(response: DebugProtocol.LaunchResponse, args: LaunchRequestArguments): void {
		this._stopOnEntry = args.stopOnEntry;
		const cwd = args.cwd ? args.cwd : process.cwd();
		const sourceRoot = args.sourceRoot ? args.sourceRoot : cwd;
		this.setupSourceEnv(sourceRoot);
		const programArg = args.args ? args.args : [];

		const port = args.port ? args.port : 21110;

		const useInternalLua = args.useInternalLua != null ? args.useInternalLua : args.program.endsWith(".lua");

		if (useInternalLua) {
			let vm = path.resolve(path.join(__dirname, '../../prebuilt/lua_with_lrdb_server.js'));
			let program = this.convertClientPathToDebugger(args.program);
			this._debug_server_process = fork(vm,
				[program].concat(programArg),
				{ cwd: cwd, silent: true });

			this._debug_client = new LRDBChildProcessClient(this._debug_server_process);
		}
		else {
			this._debug_server_process = spawn(args.program, programArg, { cwd: cwd });


			this._debug_client = new LRDBTCPClient(port, 'localhost');
		}

		this._debug_client.on_event = (event: DebugServerEvent) => { this.handleServerEvents(event) };
		this._debug_client.on_close = () => {
		};
		this._debug_client.on_error = (e: any) => {
		};

		this._debug_client.on_open = () => {
			this.sendEvent(new InitializedEvent());
		};

		this._debug_server_process.stdout.on('data', (data: any) => {
			this.sendEvent(new OutputEvent(data.toString(), 'stdout'));
		});
		this._debug_server_process.stderr.on('data', (data: any) => {
			this.sendEvent(new OutputEvent(data.toString(), 'stderr'));
		});
		this._debug_server_process.on('error', (msg: string) => {
			this.sendEvent(new OutputEvent(msg, 'error'));
		});
		this._debug_server_process.on('close', (code: number, signal: string) => {
			this.sendEvent(new OutputEvent(`exit status: ${code}\n`));
			this.sendEvent(new TerminatedEvent());
		});

		this.sendResponse(response);
	}

	protected attachRequest(response: DebugProtocol.AttachResponse, args: AttachRequestArguments): void {
		this._stopOnEntry = args.stopOnEntry;
		this.setupSourceEnv(args.sourceRoot);

		this._debug_client = new LRDBTCPClient(args.port, args.host);
		this._debug_client.on_event = (event: DebugServerEvent) => { this.handleServerEvents(event) };
		this._debug_client.on_close = () => {
			this.sendEvent(new TerminatedEvent());
		};
		this._debug_client.on_error = (e: any) => {
		};

		this._debug_client.on_open = () => {
			this.sendEvent(new InitializedEvent());
		};
		this.sendResponse(response);
	}


	protected configurationDoneRequest(response: DebugProtocol.ConfigurationDoneResponse, args: DebugProtocol.ConfigurationDoneArguments): void {
		this.sendResponse(response);
		if (this._stopOnEntry) {
			this.sendEvent(new StoppedEvent("entry", LuaDebugSession.THREAD_ID));
		} else {
			this._debug_client.send("continue");
		}
	}

	protected setBreakPointsRequest(response: DebugProtocol.SetBreakpointsResponse, args: DebugProtocol.SetBreakpointsArguments): void {

		var path = args.source.path;

		// read file contents into array for direct access
		var lines = readFileSync(path).toString().split('\n');

		var breakpoints = new Array<Breakpoint>();

		var debuggerFilePath = this.convertClientPathToDebugger(path);

		this._debug_client.send("clear_breakpoints", { "file": debuggerFilePath });
		// verify breakpoint locations
		for (let souceBreakpoint of args.breakpoints) {
			var l = this.convertClientLineToDebugger(souceBreakpoint.line);
			var verified = false;
			while (l <= lines.length) {
				const line = lines[l - 1].trim();
				// if a line is empty or starts with '--' we don't allow to set a breakpoint but move the breakpoint down
				if (line.length == 0 || line.startsWith("--")) {
					l++;
				}
				else {
					verified = true;    // this breakpoint has been validated
					break;
				}
			}
			const bp = <DebugProtocol.Breakpoint>new Breakpoint(verified, this.convertDebuggerLineToClient(l));
			bp.id = this._breakPointID++;
			breakpoints.push(bp);
			if (verified) {
				var sendbreakpoint = { "line": l, "file": debuggerFilePath, "condition": undefined, "hit_condition": undefined };
				if (souceBreakpoint.condition) {
					sendbreakpoint.condition = souceBreakpoint.condition;
				}
				if (souceBreakpoint.hitCondition) {
					sendbreakpoint.hit_condition = souceBreakpoint.hitCondition;
				}
				this._debug_client.send("add_breakpoint", sendbreakpoint);
			}
		}
		this._breakPoints.set(path, breakpoints);

		// send back the actual breakpoint positions
		response.body = {
			breakpoints: breakpoints
		};
		this.sendResponse(response);
	}

	protected threadsRequest(response: DebugProtocol.ThreadsResponse): void {

		// return the default thread
		response.body = {
			threads: [
				new Thread(LuaDebugSession.THREAD_ID, "thread 1")
			]
		};
		this.sendResponse(response);
	}

	/**
	 * Returns a fake 'stacktrace' where every 'stackframe' is a word from the current line.
	 */
	protected stackTraceRequest(response: DebugProtocol.StackTraceResponse, args: DebugProtocol.StackTraceArguments): void {

		this._debug_client.send("get_stacktrace", null, (res: any) => {
			if (res.result) {
				const startFrame = typeof args.startFrame === 'number' ? args.startFrame : 0;
				const maxLevels = typeof args.levels === 'number' ? args.levels : res.result.length - startFrame;
				const endFrame = Math.min(startFrame + maxLevels, res.result.length);
				const frames = new Array<StackFrame>();
				for (let i = startFrame; i < endFrame; i++) {
					const frame = res.result[i];	// use a word of the line as the stackframe name
					let filename = this.convertDebuggerPathToClient(frame.file)
					let source = new Source(frame.id, filename)
					if (!frame.file.startsWith("@")) {
						source.sourceReference = this._sourceHandles.create(frame.file)
					}
					frames.push(new StackFrame(i, frame.func, source,
						this.convertDebuggerLineToClient(frame.line), 0));
				}
				response.body = {
					stackFrames: frames,
					totalFrames: res.result.length
				};
			}
			else {
				response.success = false;
				response.message = "unknown error";
			}
			this.sendResponse(response);
		});

	}


	protected scopesRequest(response: DebugProtocol.ScopesResponse, args: DebugProtocol.ScopesArguments): void {

		const scopes = new Array<Scope>();
		scopes.push(new Scope("Local", this._variableHandles.create(new VariableReference("get_local_variable", { "stack_no": args.frameId, "global": false, "local": true, "upvalue": false }, [])), false));
		scopes.push(new Scope("Upvalues", this._variableHandles.create(new VariableReference("get_upvalues", { "stack_no": args.frameId, "global": false, "local": false, "upvalue": true }, [])), false));
		scopes.push(new Scope("Global", this._variableHandles.create(new VariableReference("get_global", { "stack_no": args.frameId, "global": true, "local": false, "upvalue": false }, [])), true));

		response.body = {
			scopes: scopes
		};
		this.sendResponse(response);
	}

	private createVariableObjectPath(datapath: string[]): string {
		if (datapath.length == 1) {
			return datapath[0];
		}
		else {
			return '(' + datapath[0] + ')[' + datapath.slice(1).join('][') + ']';
		}
	}

	protected variablesRequest(response: DebugProtocol.VariablesResponse, args: DebugProtocol.VariablesArguments): void {

		const id = this._variableHandles.get(args.variablesReference);

		if (id != null) {
			if (id.datapath.length == 0) {
				this._debug_client.send(id.msg_name, Object.assign({}, id.msg_param), (res: any) => {
					if (res.error) {
						response.success = false;
						response.message = res.error.message;
						this.sendResponse(response);
					}
					else {
						this.variablesRequestResponce(response, res.result, id);
					}
				});
			}
			else {
				let chunk = this.createVariableObjectPath(id.datapath);
				this._debug_client.send(id.msg_name, Object.assign({ "chunk": chunk }, id.msg_param), (res: any) => {
					if (res.error) {
						response.success = false;
						response.message = res.error.message;
						this.sendResponse(response);
					}
					else {
						this.variablesRequestResponce(response, res.result[0], id);
					}
				});
			}
		}
		else {
			response.success = false;
			this.sendResponse(response);
		}
	}
	protected stringify(value: any): string {
		if (value == null) {
			return "nil";
		}
		else if (value == undefined) {
			return "none";
		}
		else {
			return JSON.stringify(value);
		}
	}

	private variablesRequestResponce(response: DebugProtocol.VariablesResponse, variablesData: any, id: VariableReference): void {

		const variables = [];
		if (variablesData instanceof Array) {
			for (let i = 0; i < variablesData.length; ++i) {
				const typename = typeof variablesData[i];
				let k = (i + 1).toString()
				let varRef = 0;
				if (typename == "object") {
					varRef = this._variableHandles.create(new VariableReference("eval", id.msg_param, id.datapath.concat([k])));
				}
				variables.push({
					name: k,
					type: typename,
					value: this.stringify(variablesData[i]),
					variablesReference: varRef
				});
			}

		}
		else {
			for (var k in variablesData) {
				const typename = typeof variablesData[k];
				let varRef = 0;
				if (typename == "object") {
					let datakey = k;
					if (id.datapath.length) {
						datakey = '"' + k + '"'
					}
					varRef = this._variableHandles.create(new VariableReference("eval", id.msg_param, id.datapath.concat([datakey])));
				}
				variables.push({
					name: k,
					type: typename,
					value: this.stringify(variablesData[k]),
					variablesReference: varRef
				});
			}
		}
		response.body = {
			variables: variables
		};
		this.sendResponse(response);
	}

	protected continueRequest(response: DebugProtocol.ContinueResponse, args: DebugProtocol.ContinueArguments): void {
		this._debug_client.send("continue");
		this.sendResponse(response);
	}

	protected nextRequest(response: DebugProtocol.NextResponse, args: DebugProtocol.NextArguments): void {
		this._debug_client.send("step");
		this.sendResponse(response);
	}

	protected stepInRequest(response: DebugProtocol.StepInResponse, args: DebugProtocol.StepInArguments): void {
		this._debug_client.send("step_in");
		this.sendResponse(response);
	}

	protected stepOutRequest(response: DebugProtocol.StepOutResponse, args: DebugProtocol.StepOutArguments): void {
		this._debug_client.send("step_out");
		this.sendResponse(response);
	}
	protected pauseRequest(response: DebugProtocol.PauseResponse, args: DebugProtocol.PauseArguments): void {
		this._debug_client.send("pause");
		this.sendResponse(response);
	}

	protected sourceRequest(response: DebugProtocol.SourceResponse, args: DebugProtocol.SourceArguments): void {

		const id = this._sourceHandles.get(args.sourceReference);
		if (id) {
			response.body = {
				content: id
			};
		}
		this.sendResponse(response);
	}


	protected disconnectRequest(response: DebugProtocol.DisconnectResponse, args: DebugProtocol.DisconnectArguments): void {
		if (this._debug_server_process) {
			this._debug_server_process.kill();
			delete this._debug_server_process;
		}
		if (this._debug_client) {
			this._debug_client.end();
			delete this._debug_client;
		}
		this.sendResponse(response);
	}

	protected evaluateRequest(response: DebugProtocol.EvaluateResponse, args: DebugProtocol.EvaluateArguments): void {
		if (!this._debug_client) {
			response.success = false;
			this.sendResponse(response);
			return;
		}
		//		if (args.context == "watch" || args.context == "hover" || args.context == "repl") {
		let chunk = args.expression;
		this._debug_client.send("eval", { "stack_no": args.frameId, "chunk": chunk, "depth": 0 }, (res: any) => {
			if (res.result) {
				let ret = ""
				for (let r of res.result) {
					if (ret.length > 0)
						ret += '	';
					ret += this.stringify(r)
				}
				let varRef = 0;
				if (res.result.length == 1) {
					let refobj = res.result[0];
					const typename = typeof refobj;
					if (refobj && typename == "object") {
						varRef = this._variableHandles.create(new VariableReference("eval", { "stack_no": args.frameId }, [chunk]));
					}
				}
				response.body = {
					result: ret,
					variablesReference: varRef
				};

			}
			else {
				response.body = {
					result: "",
					variablesReference: 0
				};
				response.success = false;
			}
			this.sendResponse(response);
		});
		/*	}
			else {
	
				response.body = {
					result: `evaluate(context: '${args.context}', '${args.expression}')`,
					variablesReference: 0
				};
				this.sendResponse(response);
			}*/
	}

	private handleServerEvents(event: DebugServerEvent) {
		if (event.method == "paused" && event.params.reason != "entry") {
			this.sendEvent(new StoppedEvent(event.params.reason, LuaDebugSession.THREAD_ID));
		}
		else if (event.method == "running") {
			this._variableHandles.reset();
			this.sendEvent(new ContinuedEvent(LuaDebugSession.THREAD_ID));
		}
		else if (event.method == "exit") {
		}
	}
}

DebugSession.run(LuaDebugSession);
