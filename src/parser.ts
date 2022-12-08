export interface MatchCase {
  regex: string;
  groupIndex: number;
}

const matchFirstStringArg: MatchCase = {
  regex: '"(.*?)"',
  groupIndex: 1,
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

export type Catch2TokenType = keyof typeof CATCH2_TOKENS;

export const GTEST_TOKENS = {
  ["TEST"]: matchFirstStringArg,
  ["TEST_F"]: matchFirstStringArg,
  ["TEST_P"]: matchFirstStringArg,
};
export type GtestTokenType = keyof typeof GTEST_TOKENS;

export const PRESET_TOKENS = { ...CATCH2_TOKENS, ...GTEST_TOKENS };

export interface TestCaseRange {
  fromLine: number;
  toLine: number;
}

export interface TestCase<T = string> {
  token: T;
  name: string;
  range?: TestCaseRange;
  children: TestCase[];
}

export type Catch2TestCase = TestCase<Catch2TokenType>;
export type GtestTestCase = TestCase<GtestTokenType>;

export function equalFrom(s: string, pos: number, target: string) {
  let isEqual = s.length - pos >= target.length;

  for (let i = 0; i < target.length; ++i) {
    isEqual &&= s.at(pos + i) === target.at(i);
  }

  return isEqual;
}

export function getAllTokens(): Record<string, MatchCase> {
  return PRESET_TOKENS;
}

export function parseTestCasesFromText(text: string): TestCase[] {
  const retCase: TestCase = {
    token: "SCENARIO",
    name: "dummy",
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
        st.pop();
      }
    } else {
      for (const [name, matchCase] of Object.entries(getAllTokens())) {
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
          childCase.name += text.at(i) ?? "";
          if (text[i] === "(") {
            parenthesesCount += 1;
          } else if (text[i] === ")") {
            parenthesesCount -= 1;
          }

          if (parenthesesCount === 0) {
            break;
          }

          move(1);
        }

        console.log(childCase.name);

        const matches = childCase.name.match(matchCase.regex);
        childCase.name = matches ? matches.at(matchCase.groupIndex) ?? "" : "";
        st.at(-1)!.case.children.push(childCase);
        st.push({
          bracketCount: 0,
          case: childCase,
        });
      }
    }
  }

  return retCase.children;
}
