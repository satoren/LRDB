declare interface DebuggableLua {
  readonly Lua: LuaConstructor
}

interface LuaConstructor {
  new (opts: (message: string) => void): Lua
}
interface Lua {
  do_string(
    script: string,
    args: string[],
    callback: (ret: boolean) => void
  ): Promise<void>
  do_file(
    file: string,
    args: string[],
    callback: (ret: boolean) => void
  ): Promise<void>
  debug_command(command_json: string)
  error(err: string)
  delete()
}

export declare function DebuggableLuaInit(): Promise<DebuggableLua>
export default DebuggableLuaInit
