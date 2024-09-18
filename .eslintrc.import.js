module.exports = {
    plugins: ["import"],
    rules: {
      "import/no-unused-modules": "error, { unusedExports: true }",
      "typescript-eslint/no-explicit-any": "off",
      camelcase: "off",
    },
  };
  