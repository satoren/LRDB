import { isJsonRpcMessage, JsonRpcMessage } from '../JsonRpc'
import { DebugRequest, DebugClientAdapter } from '../Client'
import { TypedEventEmitter } from '../TypedEventEmitter'
import { ChildProcess, ForkOptions } from 'child_process'
import * as LuaWasm from '../LuaWasm'

export class ChildProcessAdapter implements DebugClientAdapter {
  onMessage: TypedEventEmitter<JsonRpcMessage> =
    new TypedEventEmitter<JsonRpcMessage>()
  public child: ChildProcess

  public constructor(child: ChildProcess)
  public constructor(file: string, args: string[], options?: ForkOptions)

  public constructor(
    child: ChildProcess | string,
    args?: string[],
    options?: ForkOptions,
  ) {
    if (typeof child === 'string') {
      this.child = LuaWasm.run(child, args, options)
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
