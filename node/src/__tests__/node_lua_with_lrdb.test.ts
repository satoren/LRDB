import path from 'path'
import DebuggableLuaInit from '../../bin/node_lua_with_lrdb'

import { isJsonRpcNotify } from '../JsonRpc'
import { DebugRequest } from '../Client'

const testScriptDir = path.resolve(__dirname, '../../../test/lua/')
test('exec', async () => {
  const stepinRequest: DebugRequest = {
    method: 'step_in',
    jsonrpc: '2.0',
    id: 0,
  }
  const DebuggableLua = await DebuggableLuaInit()
  const lua = new DebuggableLua.Lua((m) => {
    const message = JSON.parse(m)
    if (isJsonRpcNotify(message)) {
      if (message.method === 'paused') {
        setTimeout(() => {
          lua.debug_command(JSON.stringify(stepinRequest))
        })
      }
    }
  })

  const a = await new Promise((resolve) =>
    lua.do_file(
      path.resolve(testScriptDir, 'break_coroutine_test1.lua'),
      [],
      resolve
    )
  )
  expect(a).toBe(true)
})
