import path from 'path'
import { isJsonRpcMessage, JsonRpcMessage } from '../JsonRpc'
import { DebugRequest, DebugClientAdapter } from '../Client'
import { TypedEventEmitter } from '../TypedEventEmitter'
import child_process, { ChildProcess } from 'child_process'

const debuggableLuaModule = path.resolve(
  __dirname,
  '../../bin/ChildDebuggableLua.js'
)

const runScript = (
  file: string,
  args: string[] = [],
  options?: child_process.ForkOptions
) => child_process.fork(debuggableLuaModule, [file, ...args], options)

export class ChildProcessAdapter implements DebugClientAdapter {
  onMessage: TypedEventEmitter<JsonRpcMessage> = new TypedEventEmitter<JsonRpcMessage>()
  private _child: ChildProcess

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
      this._child = runScript(child, args, options)
    } else {
      this._child = child
    }
    this.initChildProcess()
  }
  initChildProcess() {
    setTimeout(() => {
      this.onOpen.emit()
    }, 0)
    this._child.on('message', (msg: unknown) => {
      if (isJsonRpcMessage(msg)) {
        this.onMessage.emit(msg)
      }
    })
    this._child.on('close', () => {
      this.onClose.emit()
    })
  }
  send(request: DebugRequest): boolean {
    return this._child.send(request)
  }
  end(): void {
    this._child.kill()
  }
  onClose: TypedEventEmitter<void> = new TypedEventEmitter<void>()
  onOpen: TypedEventEmitter<void> = new TypedEventEmitter<void>()
  onError: TypedEventEmitter<Error> = new TypedEventEmitter<Error>()
}
