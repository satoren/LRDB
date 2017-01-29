//
// Note: This example test is leveraging the Mocha test framework.
// Please refer to their documentation on https://mochajs.org/ for help.
//

// The module 'assert' provides assertion methods from node
import * as assert from 'assert';
import * as path from 'path';

// You can import and use all API from the 'vscode' module
// as well as import your extension to test it
import * as vscode from 'vscode';
import { DebugClient } from 'vscode-debugadapter-testsupport';
import { DebugProtocol } from 'vscode-debugprotocol';

// Defines a Mocha test suite to group tests of similar kind together
suite("Lua Debug Adapter", () => {
	const DEBUG_ADAPTER = './out/src/lrdbDebug.js';
	const PROJECT_ROOT = path.join(__dirname, '../../');
	const DATA_ROOT = path.join(PROJECT_ROOT, 'test/lua/');


	let dc: DebugClient;

	setup(() => {
		dc = new DebugClient('node', DEBUG_ADAPTER, 'lua');

		return dc.start();
	});

	teardown(() => dc.stop());


	suite('basic', () => {
		test('unknown request should produce error', done => {
			dc.send('illegal_request').then(() => {
				done(new Error("does not report error on unknown request"));
			}).catch(() => {
				done();
			});
		});
	});


	suite('initialize', () => {

		test('should return supported features', () => {
			return dc.initializeRequest().then(response => {
				assert.equal(response.body.supportsConfigurationDoneRequest, true);
			});
		});

		test('should produce error for invalid \'pathFormat\'', done => {
			dc.initializeRequest({
				adapterID: 'lua',
				linesStartAt1: true,
				columnsStartAt1: true,
				pathFormat: 'url'
			}).then(response => {
				done(new Error("does not report error on invalid 'pathFormat' attribute"));
			}).catch(err => {
				// error expected
				done();
			});
		});
	});


	suite('launch', () => {

		test('should run program to the end', () => {
			const PROGRAM = path.join(DATA_ROOT, 'loop_test.lua');

			return Promise.all([
				dc.launch({ program: PROGRAM }),
				dc.configurationSequence(),
				dc.waitForEvent('terminated')
			]);
		});

		test('should stop on entry', () => {

			const PROGRAM = path.join(DATA_ROOT, 'loop_test.lua');
			const ENTRY_LINE = 1;
			return Promise.all([
				dc.launch({ program: PROGRAM, stopOnEntry: true }),
				dc.configurationSequence(),
				dc.waitForEvent('stopped'),
				dc.assertStoppedLocation('entry', { line: ENTRY_LINE } )
			]);
		});
	});
	
	suite('breakpoint', () => {
		test('should stop on breakpoint', () => {
			const PROGRAM = path.join(DATA_ROOT, 'loop_test.lua');
			const BREAK_LINE = 5;
			return Promise.all([
				dc.hitBreakpoint({ program: PROGRAM},{path:PROGRAM,line:BREAK_LINE}),
				dc.configurationSequence(),
				dc.waitForEvent('stopped'),
				dc.assertStoppedLocation('breakpoint', { line: BREAK_LINE } ),
			]);
		});
	});
});