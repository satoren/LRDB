export type JsonRpcMessage = JsonRpcRequest | JsonRpcResponse | JsonRpcNotify

export interface JsonRpcRequest {
  jsonrpc: '2.0'
  method: string
  params?: unknown
  id: string | number
}

export function isJsonRpcRequest(
  message: JsonRpcMessage,
): message is JsonRpcRequest {
  return (
    (message as JsonRpcRequest).id != null &&
    typeof (message as JsonRpcRequest).method === 'string'
  )
}
export function isJsonRpcResponse(
  message: JsonRpcMessage,
): message is JsonRpcResponse {
  return (message as JsonRpcResponse).id != null && !isJsonRpcRequest(message)
}
export function isJsonRpcNotify(
  message: JsonRpcMessage,
): message is JsonRpcNotify {
  return (
    typeof (message as JsonRpcNotify).method === 'string' &&
    !isJsonRpcRequest(message) &&
    !isJsonRpcResponse(message)
  )
}
export function isJsonRpcMessage(v: unknown): v is JsonRpcMessage {
  return (
    isJsonRpcRequest(v as JsonRpcMessage) ||
    isJsonRpcResponse(v as JsonRpcMessage) ||
    isJsonRpcNotify(v as JsonRpcMessage)
  )
}

export interface JsonRpcResponse {
  jsonrpc: '2.0'
  result?: unknown
  error?: unknown
  id: string | number
}

export interface JsonRpcNotify {
  jsonrpc: '2.0'
  method: string
  params?: unknown
}
