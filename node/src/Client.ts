import { TypedEventTarget, TypedEventEmitter } from './TypedEventEmitter'
import {
  JsonRpcNotify,
  JsonRpcRequest,
  JsonRpcResponse,
  JsonRpcMessage,
  isJsonRpcNotify,
  isJsonRpcResponse,
} from './JsonRpc'

export type DebugRequest =
  | StepRequest
  | StepInRequest
  | StepOutRequest
  | ContinueRequest
  | PauseRequest
  | AddBreakPointRequest
  | GetBreakPointsRequest
  | ClearBreakPointsRequest
  | GetStackTraceRequest
  | GetLocalVariableRequest
  | GetUpvaluesRequest
  | EvalRequest
  | GetGlobalRequest

export interface DebugClientAdapter {
  onMessage: TypedEventTarget<JsonRpcMessage>
  onOpen: TypedEventTarget<void>
  onClose: TypedEventTarget<void>
  onError: TypedEventTarget<Error>
  send(request: DebugRequest): boolean
  end(): void
}

export class Client {
  private seqId = 0
  private currentStatus_?: RunningStatus
  constructor(private adapter: DebugClientAdapter) {
    adapter.onMessage.on((msg) => {
      if (isJsonRpcNotify(msg)) {
        const notify = msg as DebuggerNotify
        this.currentStatus_ = notify.method
        this.onNotify.emit(notify)
      }
    })

    this.onClose = adapter.onClose
    this.onOpen = adapter.onOpen
    this.onError = adapter.onError
  }
  get currentStatus(): RunningStatus | undefined {
    return this.currentStatus_
  }
  send<T extends DebugRequest>(request: T): Promise<DebugResponseType<T>> {
    const { onMessage, onError } = this.adapter
    return new Promise<DebugResponseType<T>>((resolve, reject) => {
      const onReceiveMessage = (msg: JsonRpcMessage) => {
        if (isJsonRpcResponse(msg)) {
          if (request.id === msg.id) {
            if (msg.error) {
              reject(Error(JSON.stringify(msg.error)))
            } else {
              resolve(msg as DebugResponseType<T>)
            }
            onMessage.off(onReceiveMessage)
            onError.off(onReceiveError)
          }
        }
      }
      const onReceiveError = (err: Error) => {
        reject(err)
        onMessage.off(onReceiveMessage)
        onError.off(onReceiveError)
      }

      const ret = this.adapter.send(request)
      if (ret) {
        onMessage.on(onReceiveMessage)
        onError.on(onReceiveError)
      } else {
        reject(Error('Send error'))
      }
    })
  }

  step = (): Promise<DebugResponseType<StepRequest>> =>
    this.send({ method: 'step', jsonrpc: '2.0', id: this.seqId++ })
  stepIn = (): Promise<DebugResponseType<StepInRequest>> =>
    this.send({ method: 'step_in', jsonrpc: '2.0', id: this.seqId++ })
  stepOut = (): Promise<DebugResponseType<StepOutRequest>> =>
    this.send({ method: 'step_out', jsonrpc: '2.0', id: this.seqId++ })
  continue = (): Promise<DebugResponseType<ContinueRequest>> =>
    this.send({ method: 'continue', jsonrpc: '2.0', id: this.seqId++ })
  pause = (): Promise<DebugResponseType<PauseRequest>> =>
    this.send({ method: 'pause', jsonrpc: '2.0', id: this.seqId++ })

  addBreakPoint = (
    params: AddBreakPointRequest['params'],
  ): Promise<DebugResponseType<AddBreakPointRequest>> =>
    this.send({
      method: 'add_breakpoint',
      jsonrpc: '2.0',
      id: this.seqId++,
      params,
    })
  getBreakPoints = (): Promise<DebugResponseType<GetBreakPointsRequest>> =>
    this.send({
      method: 'get_breakpoints',
      jsonrpc: '2.0',
      id: this.seqId++,
    })
  clearBreakPoints = (
    params: ClearBreakPointsRequest['params'],
  ): Promise<DebugResponseType<ClearBreakPointsRequest>> =>
    this.send({
      method: 'clear_breakpoints',
      jsonrpc: '2.0',
      id: this.seqId++,
      params,
    })
  getStackTrace = (): Promise<DebugResponseType<GetStackTraceRequest>> =>
    this.send({
      method: 'get_stacktrace',
      jsonrpc: '2.0',
      id: this.seqId++,
    })
  getLocalVariable = (
    params: GetLocalVariableRequest['params'],
  ): Promise<DebugResponseType<GetLocalVariableRequest>> =>
    this.send({
      method: 'get_local_variable',
      jsonrpc: '2.0',
      id: this.seqId++,
      params,
    })
  getUpvalues = (
    params: GetUpvaluesRequest['params'],
  ): Promise<DebugResponseType<GetUpvaluesRequest>> =>
    this.send({
      method: 'get_upvalues',
      jsonrpc: '2.0',
      id: this.seqId++,
      params,
    })
  eval = (
    params: EvalRequest['params'],
  ): Promise<DebugResponseType<EvalRequest>> =>
    this.send({
      method: 'eval',
      jsonrpc: '2.0',
      id: this.seqId++,
      params,
    })
  getGlobal = (
    params: GetGlobalRequest['params'],
  ): Promise<DebugResponseType<GetGlobalRequest>> =>
    this.send({
      method: 'get_global',
      jsonrpc: '2.0',
      id: this.seqId++,
      params,
    })

  end(): void {
    this.adapter.end()
  }

  onNotify: TypedEventEmitter<DebuggerNotify> =
    new TypedEventEmitter<DebuggerNotify>()
  onClose: TypedEventTarget<void>
  onOpen: TypedEventTarget<void>
  onError: TypedEventTarget<Error>
}

export type DebugResponse = JsonRpcResponse
export type DebuggerNotify =
  | PausedNotify
  | ConnectedNotify
  | ExitNotify
  | RunningNotify

export type RunningStatus = DebuggerNotify['method']

export interface PausedNotify extends JsonRpcNotify {
  method: 'paused'
  params: {
    reason: 'breakpoint' | 'step' | 'step_in' | 'step_out' | 'pause' | 'entry'
  }
}
export interface ConnectedNotify extends JsonRpcNotify {
  method: 'connected'
  params?: never
}
export interface ExitNotify extends JsonRpcNotify {
  method: 'exit'
  params?: never
}
export interface RunningNotify extends JsonRpcNotify {
  method: 'running'
  params?: never
}

interface StepRequest extends JsonRpcRequest {
  method: 'step'
  params?: never
}
interface StepInRequest extends JsonRpcRequest {
  method: 'step_in'
  params?: never
}
export interface StepOutRequest extends JsonRpcRequest {
  method: 'step_out'
  params?: never
}
export interface ContinueRequest extends JsonRpcRequest {
  method: 'continue'
  params?: never
}
export interface PauseRequest extends JsonRpcRequest {
  method: 'pause'
  params?: never
}
export interface AddBreakPointRequest extends JsonRpcRequest {
  method: 'add_breakpoint'
  params: {
    line: number
    file: string
    condition?: string
    hit_condition?: string
  }
}
export interface GetBreakPointsRequest extends JsonRpcRequest {
  method: 'get_breakpoints'
  params?: never
}

export interface ClearBreakPointsRequest extends JsonRpcRequest {
  method: 'clear_breakpoints'
  params: {
    file: string
  }
}
export interface GetStackTraceRequest extends JsonRpcRequest {
  method: 'get_stacktrace'
  params?: never
}
export interface GetLocalVariableRequest extends JsonRpcRequest {
  method: 'get_local_variable'
  params: {
    stack_no: number
    depth?: number
  }
}
export interface GetUpvaluesRequest extends JsonRpcRequest {
  method: 'get_upvalues'
  params: {
    stack_no: number
    depth?: number
  }
}
export interface EvalRequest extends JsonRpcRequest {
  method: 'eval'
  params: {
    chunk: string
    stack_no: number
    depth?: number
    global?: boolean
    local?: boolean
    upvalue?: boolean
  }
}
export interface GetGlobalRequest extends JsonRpcRequest {
  method: 'get_global'
  params?: {
    depth?: number
  }
}

type StackInfo = {
  file: string
  func: string
  line: number
  id: string
}

type Breakpoint = {
  line: number
  func?: string
  file: string
  condition?: string
  hit_count: number
}

type ResponceResultType = {
  get_stacktrace: StackInfo[]
  get_local_variable: Record<string, unknown>
  get_upvalues: Record<string, unknown>
  eval: unknown
  get_global: Record<string, unknown>
  step: never
  step_in: never
  step_out: never
  continue: never
  pause: never
  add_breakpoint: never
  get_breakpoints: Breakpoint[]
  clear_breakpoints: never
}

export type DebugResponseType<T extends DebugRequest> = Pick<T, 'id'> & {
  result: ResponceResultType[T['method']]
}
