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


export interface StartupCommand {
	program: string;
	args: string[];
	cwd: string;
}


export interface LaunchRequestArguments extends DebugProtocol.LaunchRequestArguments {

	startupCommand: StartupCommand;

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
	frameId: number;
	datapath: string[];

	public constructor(frameId: number, datapath: string[]) {
		this.frameId = frameId;
		this.datapath = datapath;
	}

}

class LRDBClient {
	private _connection: net.Socket;

	private _callback_map = {};
	private _request_id = 0;

	public constructor(port: number, host: string) {
		this._connection = net.connect(port, host);
		this._connection.on('connect', () => {
			console.log('Debug server connected');
			if (this.openDelegate) {
				this.openDelegate();
			}

			this._connection.on('close', () => {
				if (this.closeDelegate) {
					this.closeDelegate();
				}
			});
		});
		var retryCount = 0;
		this._connection.on('error', (e) => {
			console.log('Connection Failed - ' + host + ':' + port);
			if (retryCount < 5) {
				retryCount++;
				this._connection.setTimeout(1000, () => {
					this._connection.connect(port, host);
				});
				return;
			}
			console.error(e.message);
			if (this.errorDelegate) {
				this.errorDelegate();
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
		if (callback) {
			this._callback_map[id] = callback
		}
		var data = JSON.stringify({ "method": method, "param": param, "id": id }) + "\n";
		this._connection.write(data);
	}
	public receive(event: DebugServerEvent) {
		if (this._callback_map[event.id]) {
			this._callback_map[event.id](event);
			this._callback_map[event.id] = undefined;
		}
		if (this.eventDelegate) {
			this.eventDelegate(event);
		}
	}

	eventDelegate: (event: DebugServerEvent) => void;
	closeDelegate: () => void;
	openDelegate: () => void;
	errorDelegate: () => void;
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
		// This debug adapter implements the configurationDoneRequest.
		response.body.supportsConfigurationDoneRequest = true;

		response.body.supportsConditionalBreakpoints = true;

		// make VS Code to use 'evaluate' when hovering over source
		response.body.supportsEvaluateForHovers = true;

		this.sendResponse(response);
	}

	protected launchRequest(response: DebugProtocol.LaunchResponse, args: LaunchRequestArguments): void {
		this._stopOnEntry = args.stopOnEntry;

		var startupCommand = args.startupCommand;
		if (startupCommand) {
			var server_args = args.startupCommand.args;
			this._debug_server_process = spawn(startupCommand.program, startupCommand.args, {
				cwd: startupCommand.cwd,
				env: process.env
			});
		}

		this._debug_client = new LRDBClient(args.port, args.host);
		this._debug_client.eventDelegate = (event: DebugServerEvent) => { this.handleServerEvents(event) };
		this._debug_client.closeDelegate = () => {
			this.sendEvent(new TerminatedEvent());
		};
		this._debug_client.errorDelegate = () => {
			this.sendEvent(new TerminatedEvent());
		};

		this.convertClientLineToDebugger = (line: number): number => {
			return line;
		}
		this.convertDebuggerLineToClient = (line: number): number => {
			return line;
		}

		this.convertClientPathToDebugger = (clientPath: string): string => {
			return path.relative(args.sourceRoot, clientPath);
		}
		this.convertDebuggerPathToClient = (debuggerPath: string): string => {
			const filename: string = debuggerPath.startsWith("@") ? debuggerPath.substr(1) : debuggerPath;
			return path.join(args.sourceRoot, filename);
		}

		this._debug_client.openDelegate = () => {
			this.sendResponse(response);
			this.sendEvent(new InitializedEvent());
		};
	}

	protected configurationDoneRequest(response: DebugProtocol.ConfigurationDoneResponse, args: DebugProtocol.ConfigurationDoneArguments): void {

		if (this._stopOnEntry) {
		} else {
			this._debug_client.send("continue");
		}
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
			while (l < lines.length) {
				const line = lines[l-1].trim();
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
			this.sendResponse(response);
		});

	}


	protected scopesRequest(response: DebugProtocol.ScopesResponse, args: DebugProtocol.ScopesArguments): void {

		const scopes = new Array<Scope>();
		scopes.push(new Scope("Local", this._variableHandles.create(new VariableReference(args.frameId, ["local"])), false));
		scopes.push(new Scope("UpValue", this._variableHandles.create(new VariableReference(args.frameId, ["upvalue"])), false));
		scopes.push(new Scope("Env", this._variableHandles.create(new VariableReference(args.frameId, ["_ENV"])), true));
		scopes.push(new Scope("Global", this._variableHandles.create(new VariableReference(args.frameId, ["_G"])), true));

		response.body = {
			scopes: scopes
		};
		this.sendResponse(response);
	}

	protected variablesRequest(response: DebugProtocol.VariablesResponse, args: DebugProtocol.VariablesArguments): void {

		const id = this._variableHandles.get(args.variablesReference);

		if (id != null) {
			if (id.datapath[0] == "local") {
				if (id.datapath.length == 1) {
					this._debug_client.send("get_local_variable", { "stack_no": id.frameId }, (res: any) => {
						this.variablesRequestResponce(response, res.result, id);
					});
				}
				else {
					let chunk = 'return _ENV["' + id.datapath.slice(1).join('"]["') + '"]';
					this._debug_client.send("eval", { "stack_no": id.frameId, "chunk": chunk, "global": false, "upvalue": false }, (res: any) => {
						this.variablesRequestResponce(response, res.result[0], id);
					});
				}
			}
			else if (id.datapath[0] == "upvalue") {
				if (id.datapath.length == 1) {
					this._debug_client.send("get_upvalues", { "stack_no": id.frameId }, (res: any) => {
						this.variablesRequestResponce(response, res.result, id);
					});
				}
				else {
					let chunk = 'return _ENV["' + id.datapath.slice(1).join('"]["') + '"]';
					this._debug_client.send("eval", { "stack_no": id.frameId, "chunk": chunk, "global": false, "local": false }, (res: any) => {
						this.variablesRequestResponce(response, res.result[0], id);
					});
				}
			}
			else if (id.datapath[0] == "_ENV") {
				if (id.datapath.length == 1) {
					let chunk = 'return _ENV;'
					this._debug_client.send("eval", { "stack_no": id.frameId, "chunk": chunk }, (res: any) => {
						this.variablesRequestResponce(response, res.result[0], id);
					});
				}
				else {
					let chunk = 'return _ENV["' + id.datapath.slice(1).join('"]["') + '"]';
					this._debug_client.send("eval", { "stack_no": id.frameId, "chunk": chunk }, (res: any) => {
						this.variablesRequestResponce(response, res.result[0], id);
					});
				}
			}
			else if (id.datapath[0] == "_G") {
				if (id.datapath.length == 1) {
					let chunk = 'return _G';
					this._debug_client.send("eval", { "stack_no": id.frameId, "chunk": chunk }, (res: any) => {
						this.variablesRequestResponce(response, res.result[0], id);
					});
				}
				else {
					let chunk = 'return _G["' + id.datapath.join('"]["') + '"]';
					this._debug_client.send("eval", { "stack_no": id.frameId, "chunk": chunk }, (res: any) => {
						this.variablesRequestResponce(response, res.result[0], id);
					});

				}
			}
		}

	}

	private variablesRequestResponce(response: DebugProtocol.VariablesResponse, variablesData: any, id: VariableReference): void {

		const variables = [];
		for (var k in variablesData) {
			const typename = typeof variablesData[k];
			let varRef = 0;
			if (typename == "object") {
				varRef = this._variableHandles.create(new VariableReference(id.frameId, id.datapath.concat([k])));
			}
			variables.push({
				name: k,
				type: typename,
				value: String(variablesData[k]),
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
	}

	protected nextRequest(response: DebugProtocol.NextResponse, args: DebugProtocol.NextArguments): void {
		this._debug_client.send("step");
	}

	protected stepInRequest(response: DebugProtocol.NextResponse, args: DebugProtocol.NextArguments): void {
		this._debug_client.send("step_in");
	}

	protected stepOutRequest(response: DebugProtocol.NextResponse, args: DebugProtocol.NextArguments): void {
		this._debug_client.send("step_out");
	}

	protected disconnectRequest(response: DebugProtocol.DisconnectResponse, args: DebugProtocol.DisconnectArguments): void {
		this._debug_server_process.kill();
		this.sendResponse(response);
		this.shutdown();
	}

	protected evaluateRequest(response: DebugProtocol.EvaluateResponse, args: DebugProtocol.EvaluateArguments): void {
		if (args.context == "watch" || args.context == "hover") {
			let chunk = "return " + args.expression
			this._debug_client.send("eval", { "stack_no": args.frameId, "chunk": chunk }, (res: any) => {
				if (res.result) {
					let ret = ""
					for (let r of res.result) {
						if (ret.length > 0)
							ret += '	';
						ret += JSON.stringify(r)
					}


					response.body = {
						result: ret,
						variablesReference: 0
					};
				}
				else {
					response.body = {
						result: "",
						variablesReference: 0
					};
				}
				this.sendResponse(response);
			});
			return
		}
		response.body = {
			result: `evaluate(context: '${args.context}', '${args.expression}')`,
			variablesReference: 0
		};
		this.sendResponse(response);
	}

	private handleServerEvents(event: DebugServerEvent) {
		if (event.method == "paused") {
			this.sendEvent(new StoppedEvent(event.param.reason, LuaDebugSession.THREAD_ID));
		}
		else if (event.method == "running") {
			this.sendEvent(new ContinuedEvent(LuaDebugSession.THREAD_ID));
		}
		else if (event.method == "exit") {
			this.sendEvent(new TerminatedEvent());
		}
	}
}

DebugSession.run(LuaDebugSession);
