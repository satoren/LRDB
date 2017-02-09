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

function sequenceVariablesRequest(dc: DebugClient, varref: number, datapath: string[]) {
	let req = dc.variablesRequest({ variablesReference: varref })
	let last = datapath.pop();
	for (let p of datapath) {
		req = req.then(response => {
			for (let va of response.body.variables) {
				if (va.name == p && va.variablesReference != 0) {
					return dc.variablesRequest({ variablesReference: va.variablesReference });
				}
			}
			assert.ok(false, "not found:" + p + " in " + JSON.stringify(response.body.variables));
		});
	}
	return req.then(response => {
		for (let va of response.body.variables) {
			if (va.name == last) {
				return va;
			}
		}
		assert.ok(false, "not found:" + last + " in " + JSON.stringify(response.body.variables));
	})
}

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
				dc.assertStoppedLocation('entry', { line: ENTRY_LINE })
			]);
		});
	});

	suite('breakpoint', () => {
		test('should stop on breakpoint', () => {
			const PROGRAM = path.join(DATA_ROOT, 'loop_test.lua');
			const BREAK_LINE = 5;
			return dc.hitBreakpoint({ program: PROGRAM }, { path: PROGRAM, line: BREAK_LINE });
		});
	});
	suite('evaluate', () => {
		setup(() => {
			const PROGRAM = path.join(DATA_ROOT, 'loop_test.lua');
			return Promise.all([
					dc.launch({ program: PROGRAM, stopOnEntry: true }).then(() => dc.configurationSequence()),
					dc.waitForEvent('stopped'),
				]);
		});
		test('check evaluate results', () => {
			let evaltests = []
			evaltests.push(dc.evaluateRequest({ expression: "{{1}}", context: "watch", frameId: 0 }).then(response => {
				return sequenceVariablesRequest(dc, response.body.variablesReference, ["1", "1"]).then(va => {
					assert.equal(va.value, "1");
				});
			}));
			evaltests.push(dc.evaluateRequest({ expression: "{{{5,4}}}", context: "watch", frameId: 0 }).then(response => {
				return sequenceVariablesRequest(dc, response.body.variablesReference, ["1", "1", "2"]).then(va => {
					assert.equal(va.value, "4");
				});
			}));
			evaltests.push(dc.evaluateRequest({ expression: "{a={4,2}}", context: "watch", frameId: 0 }).then(response => {
				return sequenceVariablesRequest(dc, response.body.variablesReference, ["a", "2"]).then(va => {
					assert.equal(va.value, "2");
				});
			}));
			return Promise.all(evaltests);
		});
	});


	suite('upvalues', () => {
		
		setup(() => {
			const PROGRAM = path.join(DATA_ROOT, 'get_upvalue_test.lua');
			const BREAK_LINE = 6;
			return dc.hitBreakpoint({ program: PROGRAM , stopOnEntry: false }, { path: PROGRAM, line: BREAK_LINE });
		});
		function getUpvalueScope(frameID: number) {
			return dc.scopesRequest({ frameId: frameID }).then(response => {
				for (let scope of response.body.scopes) {
					if (scope.name == "Upvalues") {
						return scope;
					}
				}
				assert.ok(false, "upvalue not found");
			});
		}
		test('check upvalue a', () => {
			let evaltests = []

			evaltests.push(getUpvalueScope(0).then(scope => {
				return sequenceVariablesRequest(dc, scope.variablesReference, ["a"]).then(va => {
					assert.equal(va.value, "1");
				});
			}));
			return Promise.all(evaltests);
		});
		test('check upvalue table', () => {
			let evaltests = []

			//local t={{1,2,3,4},5,6}
			evaltests.push(getUpvalueScope(0).then(scope => {
				return sequenceVariablesRequest(dc, scope.variablesReference, ["t", "1", "1"]).then(va => {
					assert.equal(va.value, "1");
				});
			}));

			evaltests.push(getUpvalueScope(0).then(scope => {
				return sequenceVariablesRequest(dc, scope.variablesReference, ["t", "1", "3"]).then(va => {
					assert.equal(va.value, "3");
				});
			}));
			evaltests.push(getUpvalueScope(0).then(scope => {
				return sequenceVariablesRequest(dc, scope.variablesReference, ["t", "2"]).then(va => {
					assert.equal(va.value, "5");
				});
			}));

			return Promise.all(evaltests);
		});
	});
	
	suite('local', () => {		
		setup(() => {
			const PROGRAM = path.join(DATA_ROOT, 'get_local_variable_test.lua');
			const BREAK_LINE = 7;
			return dc.hitBreakpoint({ program: PROGRAM , stopOnEntry: false }, { path: PROGRAM, line: BREAK_LINE });
		});
		function getUpvalueScope(frameID: number) {
			return dc.scopesRequest({ frameId: frameID }).then(response => {
				for (let scope of response.body.scopes) {
					if (scope.name == "Local") {
						return scope;
					}
				}
				assert.ok(false, "upvalue not found");
			});
		}
		test('check upvalue a', () => {
			let evaltests = []

			evaltests.push(getUpvalueScope(0).then(scope => {
				return sequenceVariablesRequest(dc, scope.variablesReference, ["local_value1"]).then(va => {
					assert.equal(va.value, "1");
				});
			}));
			evaltests.push(getUpvalueScope(0).then(scope => {
				return sequenceVariablesRequest(dc, scope.variablesReference, ["local_value2"]).then(va => {
					assert.equal(va.value, '"abc"');
				});
			}));
			evaltests.push(getUpvalueScope(0).then(scope => {
				return sequenceVariablesRequest(dc, scope.variablesReference, ["local_value3"]).then(va => {
					assert.equal(va.value, "1");
				});
			}));
			evaltests.push(getUpvalueScope(0).then(scope => {
				return sequenceVariablesRequest(dc, scope.variablesReference, ["local_value4"]["1"]).then(va => {
					assert.equal(va.value, "4234.3");
				});
			}));
			return Promise.all(evaltests);
		});
	});
});