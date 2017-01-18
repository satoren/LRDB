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
			program: '${command.AskForProgramName}',
			cwd: '${workspaceRoot}',
			port: 21110,			
			stopOnEntry: true
		}
	]
}

function launchBinary(): string {
	if (os.type() == 'Windows_NT') {
		return "windows/lua_with_lrdb_server.exe"
	}
	else if (os.type() == 'Linux') {

	}
	else if (os.type() == 'Darwin') {

	}
	return ""
}

export function activate(context: vscode.ExtensionContext) {
	let disposable = vscode.commands.registerCommand('extension.lrdb.getProgramName', config => {
		return vscode.workspace.asRelativePath(vscode.window.activeTextEditor.document.fileName);
	});
	context.subscriptions.push(disposable);

	context.subscriptions.push(vscode.commands.registerCommand('extension.lrdb.getDefaultDebugServerName', config => {
		return context.asAbsolutePath(path.join('out','bin', launchBinary()));
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
