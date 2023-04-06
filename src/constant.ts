/* eslint-disable @typescript-eslint/naming-convention */
import * as vscode from "vscode";
import { PRESET_TOKENS, TOKEN_PREFIX, TestCase, TokenType } from "./parser";
import { escapeCatch2Chars, escapeShellArg } from "./terminal";

export const EXTENSION_NAME = "cmake-test-tools";

export enum Commands {
  DebugTest = "cmake-test-tools.debugTest",
  RunTest = "cmake-test-tools.runTest",
  DiscoverTests = "cmake-test-tools.discoverTests",
  GetTestParam = "cmake-test-tools.getTestParam",
}

export const GlobalVars = {
  currentTestCase: undefined as TestCase | undefined,
  getTestArgs(shouldEscape = true): string[] {
    let args: string[] = [];
    let testCase = this.currentTestCase;
    const voteForTestType: Record<TokenType, number> = {
      gtest: 0,
      catch2: 0,
    };

    while (testCase) {
      // count and vote for final test case type
      const tokenType = PRESET_TOKENS[testCase.token];
      if (tokenType) {
        voteForTestType[tokenType] += 1;
      }

      // convert token to test case name
      args.push((TOKEN_PREFIX[testCase.token] ?? "") + testCase.name);
      testCase = testCase.parent;
    }

    args.pop();
    args = args.reverse();

    // determine test case type
    const voteTokenType = Array.from(Object.entries(voteForTestType)).reduce(
      (pre, [tokenType, count]) => {
        if (count > pre.count) {
          pre.count = count;
          pre.maxVoteTokenType = tokenType as TokenType;
        }
        return pre;
      },
      {
        maxVoteTokenType: "gtest" as TokenType,
        count: 0,
      }
    );

    // for catch2
    if (voteTokenType.maxVoteTokenType === "catch2") {
      args = args
        .reduce(
          (pre, cur, i) => pre.concat(i > 0 ? "-c" : "", cur),
          [] as string[]
        )
        .filter((v) => !!v);

      args = args.map(escapeCatch2Chars);
    } else if (voteTokenType.maxVoteTokenType === "gtest") {
      args = args.map((arg) => `--gtest_filter='*${arg}*'`);
    }

    return args;
  },
};
