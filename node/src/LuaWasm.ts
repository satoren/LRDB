import * as path from 'path'
import { ChildProcess, ForkOptions, fork } from 'child_process'

export const modulePath = path.resolve(
  __dirname,
  '../bin/ChildDebuggableLua.js'
)

export const run = (
  file: string,
  args: string[] = [],
  options?: ForkOptions
): ChildProcess => fork(modulePath, [file, ...args], options)

export default { modulePath, run }
