export interface TypedEventListener<T> {
  (event: T): Promise<void> | void
}
export interface TypedEventTarget<T> {
  on: (listener: TypedEventListener<T>) => void
  once: (listener: TypedEventListener<T>) => void
  off: (listener: TypedEventListener<T>) => void
}

export class TypedEventEmitter<T> {
  private listeners: TypedEventListener<T>[] = []
  private listenersOncer: TypedEventListener<T>[] = []

  on = (listener: TypedEventListener<T>): void => {
    this.listeners.push(listener)
  }

  once = (listener: TypedEventListener<T>): void => {
    this.listenersOncer.push(listener)
  }

  off = (listener: TypedEventListener<T>): void => {
    const callbackIndex = this.listeners.indexOf(listener)
    if (callbackIndex > -1) this.listeners.splice(callbackIndex, 1)
  }

  emit = async (event: T): Promise<void> => {
    const emitted = [
      ...this.listeners.map((listener) => listener(event)),
      ...this.listenersOncer.map((listener) => listener(event)),
    ]
    this.listenersOncer = []
    await Promise.all(emitted)
  }
}
