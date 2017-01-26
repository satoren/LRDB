/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/

'use strict';

import * as vscode from 'vscode';
import * as path from 'path';
import * as os from 'os';
import * as child_process from 'child_process';

const initialConfigurations = {
	version: '0.2.0',
	configurations: [
		{
			type: 'lua',
			request: 'launch',
			name: 'Lua Launch',
			program: '${file}',
			cwd: "${workspaceRoot}",
			stopOnEntry: true
		},
		{
			type: 'lua',
			request: 'attach',
			name: 'Lua Attach',
			host: 'localhost',
			port: 21110,
			sourceRoot: "${workspaceRoot}",
			stopOnEntry: true
		}
	]
}

function launchBinary(context: vscode.ExtensionContext): string {
	if (os.type() == 'Windows_NT') {
		return path.resolve(path.join(context.extensionPath, 'out/bin/windows/lua_with_lrdb_server.exe'));
	}
	else if (os.type() == 'Linux') {
		return path.resolve(path.join(context.extensionPath, 'out/bin/linux/lua_with_lrdb_server'));
	}
	else if (os.type() == 'Darwin') {
		return path.resolve(path.join(context.extensionPath, 'out/bin/macos/lua_with_lrdb_server'));
	}
	return ""
}

export function activate(context: vscode.ExtensionContext) {
	let disposable = vscode.commands.registerCommand('extension.lrdb.getProgramName', config => {
		return vscode.window.activeTextEditor.document.fileName;
	});
	context.subscriptions.push(disposable);


	context.subscriptions.push(vscode.commands.registerCommand('extension.lrdb.provideInitialConfigurations', () => {
		return [
			'// TODO.',
			JSON.stringify(initialConfigurations, null, '\t')
		].join('\n');
	}));

	const luainterpreter = launchBinary(context);

	if (os.type() != 'Windows_NT') {
		child_process.spawn("chmod",["775",luainterpreter]);
	}

}

export function deactivate() {
}
