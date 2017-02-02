import {
	DebugSession,
	InitializedEvent, TerminatedEvent, ContinuedEvent, StoppedEvent, BreakpointEvent, OutputEvent, Event,
	Thread, StackFrame, Scope, Source, Handles, Breakpoint
} from 'vscode-debugadapter';
import { DebugProtocol } from 'vscode-debugprotocol';
import { readFileSync } from 'fs';
import { spawn, ChildProcess } from 'child_process';
import * as net from 'net';
import * as path from 'path';

import * as os from 'os';
import * as stream from 'stream';


export interface LaunchRequestArguments extends DebugProtocol.LaunchRequestArguments {

	program: string;
	args: string[];
	cwd: string;

	useEmbeddedLua?: boolean;
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
	param: any;
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

		var data = JSON.stringify({ "method": method, "param": param, "id": id }) + "\n";
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


class LRDBStdInOutClient {
	private static LRDB_IOSTREAM_PREFIX = "lrdb_stream_message:";

	private _callback_map = {};
	private _request_id = 0;

	public constructor(private input: stream.Readable, private output: stream.Writable) {

		var chunk = "";
		var ondata = (data) => {
			chunk += data.toString();
			var d_index = chunk.indexOf('\n');
			while (d_index > -1) {
				try {
					var string = chunk.substring(0, d_index);
					if (string.startsWith(LRDBStdInOutClient.LRDB_IOSTREAM_PREFIX)) {
						var json = JSON.parse(string.substr(LRDBStdInOutClient.LRDB_IOSTREAM_PREFIX.length));
						this.receive(json);
					}
					else {
						if (this.on_data) {
							this.on_data(string)
						}
					}
				}
				finally { }
				chunk = chunk.substring(d_index + 1);
				d_index = chunk.indexOf('\n');
			}
		}
		input.on('data', ondata);

		setTimeout(() => {
			if (this.on_open) {
				this.on_open();
			}
		}, 0);
	}


	public send(method: string, param?: any, callback?: (response: any) => void) {
		let id = this._request_id++;

		var data = LRDBStdInOutClient.LRDB_IOSTREAM_PREFIX + JSON.stringify({ "method": method, "param": param, "id": id }) + "\n";
		var ret = this.output.write(data);

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
		for (var key in this._callback_map) {
			this._callback_map[key]({ result: null, id: key });
			delete this._callback_map[key];
		}
		this.output.write("\r\n");
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

	private _stopOnEntry: boolean;


	private _startUpSequence: boolean;

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

	private launchBinary(): string {
		if (os.type() == 'Windows_NT') {
			return path.resolve(path.join(__dirname, '../bin', "windows/lua_with_lrdb_server.exe"));
		}
		else if (os.type() == 'Linux') {
			return path.resolve(path.join(__dirname, '../bin', "linux/lua_with_lrdb_server"));
		}
		else if (os.type() == 'Darwin') {
			return path.resolve(path.join(__dirname, '../bin', "macos/lua_with_lrdb_server"));
		}
		return ""
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
			const filename: string = debuggerPath.startsWith("@") ? debuggerPath.substr(1) : debuggerPath;
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
		const program = this.convertClientPathToDebugger(args.program);
		const programArg = args.args ? args.args : [];


		const port = args.port ? args.port : 21110;

		const useEmbeddedLua = args.useEmbeddedLua != null ? args.useEmbeddedLua : args.program.endsWith(".lua");


		if (useEmbeddedLua) {
			this._debug_server_process = spawn(this.launchBinary(),
				['--port', port.toString(), program].concat(programArg),
				{ cwd: cwd });
		}
		else {
			this._debug_server_process = spawn(args.program, programArg, { cwd: cwd });
		}
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


		this._debug_client = new LRDBTCPClient(port, 'localhost');
		this._debug_client.on_event = (event: DebugServerEvent) => { this.handleServerEvents(event) };
		this._debug_client.on_close = () => {
		};
		this._debug_client.on_error = (e: any) => {
		};

		this._debug_client.on_open = () => {
			this.sendResponse(response);
			this.sendEvent(new InitializedEvent());
		};
		this._startUpSequence = true;
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
			this.sendResponse(response);
			this.sendEvent(new InitializedEvent());
		};
		this._startUpSequence = true;
	}


	protected configurationDoneRequest(response: DebugProtocol.ConfigurationDoneResponse, args: DebugProtocol.ConfigurationDoneArguments): void {

		if (this._stopOnEntry) {
			this.sendEvent(new StoppedEvent("entry", LuaDebugSession.THREAD_ID));
		} else {
			this._debug_client.send("continue");
		}
		this._startUpSequence = false;
		this.sendResponse(response);
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
					frames.push(new StackFrame(i, frame.func, new Source(path.basename(frame.file),
						this.convertDebuggerPathToClient(frame.file)),
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
		return "_ENV" + '["' + datapath.join('"]["') + '"]'
	}

	protected variablesRequest(response: DebugProtocol.VariablesResponse, args: DebugProtocol.VariablesArguments): void {

		const id = this._variableHandles.get(args.variablesReference);

		if (id != null) {
			if (id.datapath.length == 0) {
				this._debug_client.send(id.msg_name, Object.assign({}, id.msg_param), (res: any) => {
					this.variablesRequestResponce(response, res.result, id);
				});
			}
			else {
				let chunk = "return " + this.createVariableObjectPath(id.datapath);
				this._debug_client.send(id.msg_name, Object.assign({ "chunk": chunk }, id.msg_param), (res: any) => {
					this.variablesRequestResponce(response, res.result[0], id);
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
			return "undefined";
		}
		else {
			return JSON.stringify(value);
		}
	}

	private variablesRequestResponce(response: DebugProtocol.VariablesResponse, variablesData: any, id: VariableReference): void {

		const variables = [];
		for (var k in variablesData) {
			const typename = typeof variablesData[k];
			let varRef = 0;
			if (typename == "object") {
				varRef = this._variableHandles.create(new VariableReference("eval", id.msg_param, id.datapath.concat([k])));
			}
			variables.push({
				name: k,
				type: typename,
				value: this.stringify(variablesData[k]),
				variablesReference: varRef
			});

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
		this.sendResponse(response);
	}


	protected disconnectRequest(response: DebugProtocol.DisconnectResponse, args: DebugProtocol.DisconnectArguments): void {
		if (this._debug_server_process) {
			this._debug_server_process.disconnect();
			delete this._debug_server_process;
		}
		if (this._debug_client) {
			this._debug_client.end();
			delete this._debug_client;
		}
		this.sendResponse(response);
	}

	protected evaluateRequest(response: DebugProtocol.EvaluateResponse, args: DebugProtocol.EvaluateArguments): void {
		//		if (args.context == "watch" || args.context == "hover" || args.context == "repl") {

		let chunk = "";
		if (args.context == "repl") {
			chunk = args.expression
		}
		else {
			chunk = args.expression.trim();
			if (!chunk.startsWith("return")) {
				chunk = "return " + args.expression
			}
		}
		this._debug_client.send("eval", { "stack_no": args.frameId, "chunk": chunk }, (res: any) => {
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
						varRef = this._variableHandles.create(new VariableReference("eval", { "stack_no": args.frameId }, [args.expression]));
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
		if (this._startUpSequence) {
			return;
		}
		if (event.method == "paused") {
			this.sendEvent(new StoppedEvent(event.param.reason, LuaDebugSession.THREAD_ID));
		}
		else if (event.method == "running") {
			this.sendEvent(new ContinuedEvent(LuaDebugSession.THREAD_ID));
		}
		else if (event.method == "exit") {
		}
	}
}

DebugSession.run(LuaDebugSession);
