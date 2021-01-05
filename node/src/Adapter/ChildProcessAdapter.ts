import path from 'path'
import { isJsonRpcMessage, JsonRpcMessage } from '../JsonRpc'
import { DebugRequest, DebugClientAdapter } from '../Client'
import { TypedEventEmitter } from '../TypedEventEmitter'
import child_process, { ChildProcess } from 'child_process'

export const debuggableLuaModulePath = path.resolve(
  __dirname,
  '../../bin/ChildDebuggableLua.js'
)

export const runLuaAtWasm = (
  file: string,
  args: string[] = [],
  options?: child_process.ForkOptions
) => child_process.fork(debuggableLuaModulePath, [file, ...args], options)

export class ChildProcessAdapter implements DebugClientAdapter {
  onMessage: TypedEventEmitter<JsonRpcMessage> = new TypedEventEmitter<JsonRpcMessage>()
  public child: ChildProcess

  public constructor(child: ChildProcess)
  public constructor(
    file: string,
    args: string[],
    options?: child_process.ForkOptions
  )

  public constructor(
    child: ChildProcess | string,
    args?: string[],
    options?: child_process.ForkOptions
  ) {
    if (typeof child === 'string') {
      this.child = runLuaAtWasm(child, args, options)
    } else {
      this.child = child
    }
    this.initChildProcess()
  }
  initChildProcess(): void {
    setTimeout(() => {
      this.onOpen.emit()
    }, 0)
    this.child.on('message', (msg: unknown) => {
      if (isJsonRpcMessage(msg)) {
        this.onMessage.emit(msg)
      }
    })
    this.child.on('close', () => {
      this.onClose.emit()
    })
  }
  send(request: DebugRequest): boolean {
    return this.child.send(request)
  }
  end(): void {
    this.child.kill()
  }
  onClose: TypedEventEmitter<void> = new TypedEventEmitter<void>()
  onOpen: TypedEventEmitter<void> = new TypedEventEmitter<void>()
  onError: TypedEventEmitter<Error> = new TypedEventEmitter<Error>()
}
