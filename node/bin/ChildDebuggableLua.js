const DebuggableLuaInit = require('./node_lua_with_lrdb.js');
const process = require('process');


DebuggableLuaInit().then((D) => {
  const lua = new D.Lua((m) => {
    if (process.send) {
      const message = JSON.parse(m)
      process.send(message)
    }
  })

  process.on('message', (m) => {
    lua.debug_command(JSON.stringify(m))
  })

  lua.do_file(process.argv[2], process.argv.slice(3),()=>{
    lua.exit()
  })
})
