module.exports = {
    env: {
      es6: true,
    },
    globals: {
      JSX: "readonly",
      Code: true,
    },
    extends: [
      "eslint:recommended",
      "next",
      "plugin:@typescript-eslint/recommended",
    ],
    plugins: ["eslint-plugin-prettier"],
    rules: {
      indent: "off",
      quote: 0,
      "react-hooks/rules-of-hooks": "error",
      "react-hooks/exhaustive-deps": "warn",
      "comma-dangle": ["warn", "never"],
      "max-len": ["error", { code: 400 }],
      "consistent-return": 0,
      "no-unrestricted-syntax": 0,
      "import/prefer-default-export": 0,
      "@typescript-eslint/no-shadow": [
        "error",
        { builtinGlobals: false, hoist: "never", allow: [] },
      ],
      "react/no-unescaped-entities": 0,
      "react/jsx-one-expression-per-line": 0,
      "no-console": ["error", { allow: ["warn", "error"] }],
      "operator-linebreak": "off",
      "@typescript-eslint/no-unused-vars": ["error", { args: "none" }],
      "@typescript-eslint/no-explicit-any": "warn",
      "valid-typeof": "off",
    },
  };
  