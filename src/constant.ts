/* eslint-disable @typescript-eslint/naming-convention */
import * as vscode from "vscode";
import { TestCase } from "./parser";

export const EXTENSION_NAME = "cmake-test-tools";

export enum Commands {
  DebugTest = "cmake-test-tools.debugTest",
  RunTest = "cmake-test-tools.runTest",
  DiscoverTests = "cmake-test-tools.discoverTests",
  GetTestParam = "cmake-test-tools.getTestParam",
}

export const GlobalVars = {
  currentTestCase: undefined as TestCase | undefined,
  getTestArgs() {
    let args: string[] = [];
    let testCase = this.currentTestCase;

    while (testCase) {
      args.push(testCase.name);
      testCase = testCase.parent;
    }

    args.pop();

    return args.reverse().map((v) => `"${v}"`);
  },
};
