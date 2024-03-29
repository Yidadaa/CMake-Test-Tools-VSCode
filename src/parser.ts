export interface MatchCase {
  regex: string;
  groupIndex: number;
}

const matchFirstStringArg: MatchCase = {
  regex: '"(.*?)"',
  groupIndex: 1,
};

const matchAllString: MatchCase = {
  regex: ".*",
  groupIndex: 0,
};

const matchSecondParam: MatchCase = {
  regex: `\(.*,(.*)\)`,
  groupIndex: 2,
};

export const CATCH2_TOKENS = {
  ["SCENARIO_METHOD"]: matchFirstStringArg,
  ["SCENARIO"]: matchFirstStringArg,
  ["GIVEN"]: matchFirstStringArg,
  ["AND_GIVEN"]: matchFirstStringArg,
  ["WHEN"]: matchFirstStringArg,
  ["AND_WHEN"]: matchFirstStringArg,
  ["THEN"]: matchFirstStringArg,
  ["AND_THEN"]: matchFirstStringArg,
  ["TEST_CASE"]: matchFirstStringArg,
  ["TEST_CASE_METHOD"]: matchFirstStringArg,
  ["SECTION"]: matchFirstStringArg,
};

export const TOKEN_PREFIX: Record<string, string> = {
  ["SCENARIO_METHOD"]: "Scenario: ",
  ["SCENARIO"]: "Scenario: ",
  ["GIVEN"]: "    Given: ",
  ["AND_GIVEN"]: "And given: ",
  ["WHEN"]: "     When: ",
  ["AND_WHEN"]: " And when: ",
  ["THEN"]: "     Then: ",
  ["AND_THEN"]: "      And: ",
  ["TEST_CASE"]: "",
  ["TEST_CASE_METHOD"]: "",
  ["SECTION"]: "",
};

export type Catch2TokenType = keyof typeof CATCH2_TOKENS;

export const GTEST_TOKENS = {
  ["TEST_F"]: matchSecondParam,
  ["TEST_P"]: matchSecondParam,
  ["TEST"]: matchSecondParam,
};
export type GtestTokenType = keyof typeof GTEST_TOKENS;

const TOKEN_TYPES = ["gtest", "catch2"] as const;
export type TokenType = typeof TOKEN_TYPES[number];

/**
 * All preset tokens and its type
 */
export const PRESET_TOKENS = [GTEST_TOKENS, CATCH2_TOKENS].reduce(
  (pre, cur, i) => {
    return Object.assign(
      pre,
      Object.fromEntries(Object.keys(cur).map((t) => [t, TOKEN_TYPES[i]]))
    );
  },
  {} as Record<string, TokenType>
);

/**
 * All avaliable match tokens sorted by length, from long to short.
 */
export const ALL_TOKENS = Object.entries({
  ...GTEST_TOKENS,
  ...CATCH2_TOKENS,
})
  .map(([token, matchCase]) => ({ token, matchCase }))
  .sort(
    (a, b) => b.token.length - a.token.length || b.token.localeCompare(a.token)
  );

export interface TestCaseRange {
  fromLine: number;
  toLine: number;
}

export interface TestCase<T = string> {
  token: T;
  name: string;
  range?: TestCaseRange;
  children: TestCase[];
  parent?: TestCase;
}

export type Catch2TestCase = TestCase<Catch2TokenType>;
export type GtestTestCase = TestCase<GtestTokenType>;

export function equalFrom(s: string, pos: number, target: string) {
  let isEqual = s.length - pos >= target.length;

  let i = 0;
  for (; i < target.length; ++i) {
    isEqual &&= s.at(pos + i) === target.at(i);
  }

  // determine whether token ends with stop symbol
  isEqual &&=
    i >= target.length && [" ", "(", undefined].includes(s.at(pos + i));

  return isEqual;
}

export function getAllTokens(): Array<{ token: string; matchCase: MatchCase }> {
  return ALL_TOKENS;
}

export function parseTestCasesFromText(text: string): TestCase[] {
  const retCase: TestCase = {
    token: "SCENARIO",
    name: "root",
    children: [],
  };

  type StackItem = {
    bracketCount: number;
    case: TestCase;
  };

  // Use stack to match bracketks
  const st: StackItem[] = [
    {
      bracketCount: 0,
      case: retCase,
    },
  ];

  // Parse and Match brackets
  let lineCount = 0;
  let posCount = 0;
  let i = 0;

  const move = (step: number) => {
    for (let j = 0; j < step; j += 1) {
      if (text[i] === "\n") {
        lineCount += 1;
        posCount = 0;
      }

      i += 1;
      posCount += 1;
    }
  };

  for (; i < text.length; move(1)) {
    if (text[i] === "{") {
      st.at(-1)!.bracketCount += 1;
    } else if (text[i] === "}") {
      st.at(-1)!.bracketCount -= 1;

      // Match to end of bracket, should pop from stack
      if (st.at(-1)?.bracketCount === 0) {
        if (st.at(-1)?.case.range) {
          st.at(-1)!.case.range!.toLine = lineCount;
        }
        if (st.at(-1)!.case !== retCase) {
          st.pop();
        }
      }
    } else {
      for (const tokenGroup of getAllTokens()) {
        const name = tokenGroup.token;
        const matchCase = tokenGroup.matchCase;

        if (!equalFrom(text, i, name)) {
          continue;
        }

        // Skip token
        move(name.length);

        const childCase: TestCase = {
          token: name,
          name: "",
          range: {
            fromLine: lineCount,
            toLine: lineCount,
          },
          children: [],
          parent: st.at(-1)?.case,
        };

        // Match name in next ()
        while (i < text.length && text[i] !== "(") {
          move(1);
        }

        if (text[i] === "(") {
          move(1);
        }

        // Match `)`
        let parenthesesCount = 1;
        while (i < text.length) {
          if (text[i] === "(") {
            parenthesesCount += 1;
          } else if (text[i] === ")") {
            parenthesesCount -= 1;
          }

          if (parenthesesCount === 0) {
            break;
          } else {
            childCase.name += text.at(i) ?? "";
          }

          move(1);
        }

        const matches = childCase.name.match(matchCase.regex);
        childCase.name = matches ? matches.at(matchCase.groupIndex) ?? "" : "";
        childCase.name = childCase.name.trim();

        if (childCase.name) {
          st.at(-1)!.case.children.push(childCase);
          st.push({
            bracketCount: 0,
            case: childCase,
          });
        }
      }
    }
  }

  return retCase.children;
}
