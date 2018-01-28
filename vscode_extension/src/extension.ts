'use strict';

import * as vscode from 'vscode';
import * as path from 'path';
import * as os from 'os';
import * as child_process from 'child_process';

import { localize } from './utilities';


export function activate(context: vscode.ExtensionContext) {
	context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider('lrdb', new LRDBConfigurationProvider()));
}

export function deactivate() {
}



class LRDBConfigurationProvider implements vscode.DebugConfigurationProvider {
	
		/**
		 * Returns an initial debug configuration based on contextual information, e.g. package.json or folder.
		 */
		provideDebugConfigurations(folder: vscode.WorkspaceFolder | undefined, token?: vscode.CancellationToken): vscode.ProviderResult<vscode.DebugConfiguration[]> {
			return [createLaunchConfigFromContext(folder, false)];
		}
	
		/**
		 * Try to add all missing attributes to the debug configuration being launched.
		 */
		resolveDebugConfiguration(folder: vscode.WorkspaceFolder | undefined, config: vscode.DebugConfiguration, token?: vscode.CancellationToken): vscode.ProviderResult<vscode.DebugConfiguration> {
	
			// if launch.json is missing or empty
			if (!config.type && !config.request && !config.name) {
	
				config = createLaunchConfigFromContext(folder, true);
	
				if (!config.program) {
					const message = "Cannot find a program to debug";
					return vscode.window.showInformationMessage(message).then(_ => {
						return undefined;	// abort launch
					});
				}
			}
	
			// make sure that config has a 'cwd' attribute set
			if (!config.cwd) {
				if (folder) {
					config.cwd = folder.uri.fsPath;
				} else if (config.program) {
					// derive 'cwd' from 'program'
					config.cwd = path.dirname(config.program);
				}
			}
	
			return config;
		}
	
	}
	
	
	function createLaunchConfigFromContext(folder: vscode.WorkspaceFolder | undefined, resolve: boolean): vscode.DebugConfiguration {
	
		const config = {
			type: 'lrdb',
			request: 'launch',
			name: localize('lrdb.launch.config.name', "Launch Lua program")
		};
	
		let program: string | undefined;
		
		// try to use file open in editor
		const editor = vscode.window.activeTextEditor;
		if (editor) {
			const languageId = editor.document.languageId;
			if (languageId === 'lua') {
				const wf = vscode.workspace.getWorkspaceFolder(editor.document.uri);
				if (wf === folder) {
					program = vscode.workspace.asRelativePath(editor.document.uri);
					if (!path.isAbsolute(program)) {
						program = '${workspaceFolder}/' + program;
					}
				}
			}
		}
	
		// if we couldn't find a value for 'program', we just let the launch config use the file open in the editor
		if (!resolve && !program) {
			program = '${file}';
		}
	
		if (program) {
			config['program'] = program;
		}
	
	
		return config;
	}
	
	