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

const a = `
// "BDD-style" convenience wrappers
#define SCENARIO( ... ) TEST_CASE( "Scenario: " __VA_ARGS__ )
#define SCENARIO_METHOD( className, ... ) INTERNAL_CATCH_TEST_CASE_METHOD( className, "Scenario: " __VA_ARGS__ )
#define GIVEN( desc )     INTERNAL_CATCH_DYNAMIC_SECTION( "    Given: " << desc )
#define AND_GIVEN( desc ) INTERNAL_CATCH_DYNAMIC_SECTION( "And given: " << desc )
#define WHEN( desc )      INTERNAL_CATCH_DYNAMIC_SECTION( "     When: " << desc )
#define AND_WHEN( desc )  INTERNAL_CATCH_DYNAMIC_SECTION( " And when: " << desc )
#define THEN( desc )      INTERNAL_CATCH_DYNAMIC_SECTION( "     Then: " << desc )
#define AND_THEN( desc )  INTERNAL_CATCH_DYNAMIC_SECTION( "      And: " << desc )
`;

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
  ["TEST"]: matchAllString,
  ["TEST_F"]: matchAllString,
  ["TEST_P"]: matchAllString,
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

  for (let i = 0; i < target.length; ++i) {
    isEqual &&= s.at(pos + i) === target.at(i);
  }

  return isEqual;
}

export function getAllTokens(): Record<string, MatchCase> {
  return { ...GTEST_TOKENS, ...CATCH2_TOKENS };
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
