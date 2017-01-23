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
			name: 'Lua Launch',
			program: '${command.CurrentSource}',
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
}

export function deactivate() {
}
