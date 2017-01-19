/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/

'use strict';

import * as vscode from 'vscode';
import * as path from 'path';
import * as os from 'os';

const initialConfigurations = {
	version: '0.2.0',
	configurations: [
		{
			type: 'lua',
			request: 'launch',
			name: 'Lua-Debug',
			host: 'localhost',
			port: 21110,
			sourceRoot: "${workspaceRoot}",
			startupCommand: {
				program: '${command.DefaultLuaInterpreter}',
				args: [
					'--port',
					'21110',
					'${command.CurrentSource}'
				],
				cwd: "${workspaceRoot}"
			},
			stopOnEntry: true
		}
	]
}

function launchBinary(): string {
	if (os.type() == 'Windows_NT') {
		return "windows/lua_with_lrdb_server.exe"
	}
	else if (os.type() == 'Linux') {
		return "linux/lua_with_lrdb_server.exe"
	}
	else if (os.type() == 'Darwin') {
		return "macos/lua_with_lrdb_server.exe"
	}
	return ""
}

export function activate(context: vscode.ExtensionContext) {
	let disposable = vscode.commands.registerCommand('extension.lrdb.getProgramName', config => {
		return vscode.workspace.asRelativePath(vscode.window.activeTextEditor.document.fileName);
	});
	context.subscriptions.push(disposable);

	context.subscriptions.push(vscode.commands.registerCommand('extension.lrdb.getDefaultDebugServerName', config => {
		return context.asAbsolutePath(path.join('out', 'bin', launchBinary()));
	}));

	context.subscriptions.push(vscode.commands.registerCommand('extension.lrdb.provideInitialConfigurations', () => {
		return [
			'// TODO.',
			JSON.stringify(initialConfigurations, null, '\t')
		].join('\n');
	}));
}

export function deactivate() {
}
