import { isJsonRpcMessage, JsonRpcMessage } from '../JsonRpc'
import { DebugRequest, DebugClientAdapter } from '../Client'
import { TypedEventEmitter } from '../TypedEventEmitter'
import { ChildProcess } from 'child_process'

export class ChildProcessAdapter implements DebugClientAdapter {
  onMessage: TypedEventEmitter<JsonRpcMessage> = new TypedEventEmitter<JsonRpcMessage>()
  public constructor(private _child: ChildProcess) {
    setTimeout(() => {
      this.onOpen.emit()
    }, 0)
    _child.on('message', (msg: unknown) => {
      if (isJsonRpcMessage(msg)) {
        this.onMessage.emit(msg)
      }
    })
    _child.on('close', () => {
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
