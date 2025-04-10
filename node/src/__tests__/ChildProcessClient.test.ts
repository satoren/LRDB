import * as path from 'path'
import * as ChildProcess from 'child_process'
import { ChildProcessAdapter } from '../Adapter/ChildProcessAdapter'
import { Client, DebuggerNotify, RunningStatus } from '../Client'

async function wait(
  client: Client,
  stat: RunningStatus[] | RunningStatus,
): Promise<void> {
  return new Promise<void>((resolve, reject) => {
    const handleNotify = (notify: DebuggerNotify) => {
      if (
        notify.method === stat ||
        (Array.isArray(stat) && stat.includes(notify.method))
      ) {
        resolve()
        client.onNotify.off(handleNotify)
      }
      if (notify.method === 'exit') {
        reject()
        client.onNotify.off(handleNotify)
      }
    }
    client.onNotify.on(handleNotify)
  })
}

const debuggableLuaModule = path.resolve(
  __dirname,
  '../../bin/ChildDebuggableLua.js',
)
const testScriptDir = path.resolve(__dirname, '../../../test/lua/')
const runScript = (file: string, args: string[] = []) =>
  ChildProcess.fork(debuggableLuaModule, [file, ...args], {
    cwd: testScriptDir,
  })
describe('basic', () => {
  describe.each([
    ['table.lua'],
    ['require_module.lua'],
    ['loop_test.lua'],
    ['step_in_test1.lua'],
    ['break_coroutine_test1.lua'],
    ['vaarg_test1.lua'],
  ])('script %s', (script) => {
    test('exec', async () => {
      const client = new Client(
        new ChildProcessAdapter(script, [], {
          cwd: testScriptDir,
        }),
      )
      await wait(client, 'paused')
      client.end()
    })
    test('step', async () => {
      const child = runScript(script)
      const client = new Client(new ChildProcessAdapter(child))
      await wait(client, 'paused')

      const ret = []
      while (client.currentStatus !== 'exit') {
        ret.push(await client.getStackTrace())
        await client.step()
        await wait(client, ['paused', 'exit'])
      }
      expect(ret).toMatchSnapshot()

      client.end()
      child.kill()
    })
    test('stepIn', async () => {
      const child = runScript(script)
      const client = new Client(new ChildProcessAdapter(child))
      await wait(client, 'paused')

      const ret = []
      while (client.currentStatus !== 'exit') {
        ret.push(await client.getStackTrace())
        await client.stepIn()
        await wait(client, ['paused', 'exit'])
      }
      expect(ret).toMatchSnapshot()

      client.end()
      child.kill()
    })

    test('continue', async () => {
      const child = runScript(script)
      const client = new Client(new ChildProcessAdapter(child))
      await wait(client, 'paused')

      await client.continue()
      await wait(client, ['paused', 'exit'])
      expect(client.currentStatus).toBe('exit')
      client.end()
      child.kill()
    })
  })
})

describe.each([['step_in_test1.lua'], ['vaarg_test1.lua']])(
  'script %s',
  (script) => {
    test('getLocalVariable', async () => {
      const child = runScript(script, ['arg1', 'arg2'])
      const client = new Client(new ChildProcessAdapter(child))
      await wait(client, 'paused')

      const ret = []
      while (client.currentStatus !== 'exit') {
        const localVars = await Promise.all(
          (await client.getStackTrace()).result.map((stack, index) =>
            client.getLocalVariable({ stack_no: index }),
          ),
        )
        ret.push(localVars)
        await client.stepIn()
        await wait(client, ['paused', 'exit'])
      }
      expect(ret).toMatchSnapshot()

      client.end()
      child.kill()
    })
  },
)

test('pause in infinite loop', async () => {
  const child = runScript('infinite_loop.lua', [])
  const client = new Client(new ChildProcessAdapter(child))
  await wait(client, 'paused')
  await client.continue()
  await new Promise((resolve) => setTimeout(resolve, 1))
  await client.pause()
  client.end()
  child.kill()
})

test('process exit at end', async () => {
  const child = runScript('loop_test.lua', [])
  const client = new Client(new ChildProcessAdapter(child))
  await wait(client, 'paused')
  await client.continue()
  await new Promise<void>((resolve) => child.on('exit', resolve))
  client.end()
})

test('add breakpoint', async () => {
  const child = runScript('loop_test.lua', [])
  const client = new Client(new ChildProcessAdapter(child))
  await wait(client, 'paused')
  await client.addBreakPoint({ file: 'loop_test.lua', line: 11 })
  await client.continue()
  await wait(client, ['paused', 'exit'])
  const ret = []
  while (client.currentStatus !== 'exit') {
    ret.push(await client.getStackTrace())
    await client.continue()
    await wait(client, ['paused', 'exit'])
  }
  expect(ret).toMatchSnapshot()

  client.end()
  child.kill()
})

test('get global', async () => {
  const child = runScript('test1.lua', [])
  const client = new Client(new ChildProcessAdapter(child))
  await wait(client, 'paused')
  await client.addBreakPoint({ file: 'test1.lua', line: 3 })
  await client.continue()
  await wait(client, ['paused', 'exit'])

  expect(
    (await client.getGlobal({ depth: 1 })).result['_VERSION'],
  ).toMatchSnapshot()

  client.end()
  child.kill()
})
test('get upvalues', async () => {
  const child = runScript('test1.lua', [])
  const client = new Client(new ChildProcessAdapter(child))
  await wait(client, 'paused')
  await client.addBreakPoint({ file: 'test1.lua', line: 3 })
  await client.continue()
  await wait(client, ['paused', 'exit'])

  expect(await client.getUpvalues({ stack_no: 0 })).toMatchSnapshot()

  client.end()
  child.kill()
})

test('eval', async () => {
  const child = runScript('eval_test1.lua', ['arg1', 'arg2'])
  const client = new Client(new ChildProcessAdapter(child))
  await wait(client, 'paused')
  await client.addBreakPoint({ file: 'eval_test1.lua', line: 4 })
  await client.continue()
  await wait(client, 'paused')
  const evalResult = await client.eval({
    chunk: 'arg,value,local_value,local_value3',
    stack_no: 0,
  })
  expect(evalResult).toMatchSnapshot()

  client.end()
  child.kill()
})

test('error response', async () => {
  const child = runScript('eval_test1.lua', ['arg1', 'arg2'])
  const client = new Client(new ChildProcessAdapter(child))
  await wait(client, 'paused')
  await client.addBreakPoint({ file: 'eval_test1.lua', line: 4 })
  await client.continue()
  await wait(client, 'paused')
  await expect(
    client.eval({ chunk: 'arg', stack_no: 333 }),
  ).rejects.toThrowError()

  client.end()
  child.kill()
})

test('get breakpoints', async () => {
  const child = runScript('test1.lua', [])
  const client = new Client(new ChildProcessAdapter(child))
  await wait(client, 'paused')
  await client.addBreakPoint({ file: 'test1.lua', line: 3 })
  await client.addBreakPoint({ file: 'test1.lua', line: 5 })

  expect(await (await client.getBreakPoints()).result).toMatchObject([
    { file: 'test1.lua', line: 3 },
    { file: 'test1.lua', line: 5 },
  ])

  client.end()
  child.kill()
})
test('get breakpoints', async () => {
  const child = runScript('test1.lua', [])
  const client = new Client(new ChildProcessAdapter(child))
  await wait(client, 'paused')
  await client.addBreakPoint({ file: 'test1.lua', line: 3 })
  await client.addBreakPoint({ file: 'test1.lua', line: 5 })
  await client.clearBreakPoints({ file: 'test1.lua' })
  await client.continue()
  await wait(client, ['exit'])
  client.end()
  child.kill()
})
test('get step_out', async () => {
  const child = runScript('step_out_test1.lua', [])
  const client = new Client(new ChildProcessAdapter(child))
  await wait(client, 'paused')
  await client.addBreakPoint({ file: 'step_out_test1.lua', line: 3 })
  await client.continue()
  await wait(client, 'paused')
  await client.stepOut()
  await wait(client, 'paused')
  expect(await client.getStackTrace()).toMatchSnapshot()

  client.end()
  child.kill()
})
