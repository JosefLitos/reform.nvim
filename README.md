# reform.nvim

Documentation should be informative, concise, and easy to read.
Reform the looks of your lsp documentation with a fast single-pass parser written in `C` and
further enhance your experience by enabling clickable links (fully customizable).

## Installation

### [`lazy.nvim`](https://github.com/folke/lazy.nvim)

```lua
return {
  "JosefLitos/reform.nvim",
  event = "VeryLazy",
  build = "make",
  config = true -- automatically call reform.setup(), use [opts] to customize passed table
}
```

### [`packer.nvim`](https://github.com/wbthomason/packer.nvim)

```lua
use {
  "JosefLitos/reform.nvim",
  config = [[require'reform'.setup()]],
  run = "make",
}
```

## Goals

### Features

- [x] identical look across different languages (including vim docs in lua)
- [x] as fast as possible - formatted on a single pass, written in `C`
- [x] fully customizable - all functions can be replaced by your own
- [x] links should be clickable like in any other IDE (see `open_link` bellow)
- [ ] support Rust, go
- [x] supports cmp-nvim-lsp-signature-help - has to replace internal method to inject formatting

## Supported langauges

Language servers bellow were tested.

- Bash: `bashls`
- C/Cpp: `clangd`
- Java: [`nvim-jdtls`](https://github.com/mfussenegger/nvim-jdtls)
- Lua: `sumneko_lua`/`lua-language-server` with [`neodev`](https://github.com/folke/neodev.nvim)
- Typescript/Javascript: `typescript-launguage-server`

<details><summary>

### Screenshots: `with TS` vs. `reform.nvim`

</summary>

- C/Cpp ![C/Cpp](https://user-images.githubusercontent.com/54900518/212124528-7fa9b0b1-9a2e-4b78-be81-e97ace003836.png)
- Java ![Java](https://user-images.githubusercontent.com/54900518/212200591-deb797c5-c798-4d31-b8c2-3df1a3b9e17b.png)
- Lua, including Vim-style documentation ![Lua](https://user-images.githubusercontent.com/54900518/212195668-8463fadf-a0c4-4a4e-b70a-3612a332fead.png)
</details>

## Config

Defaults:

```lua
require'reform'.setup {
  docmd = true|{          -- reform the lsp documentation output
    override = {
      convert = true|fun(), -- reform markdown/docs parser - v.l.u.convert_input_to_markdown_lines
      stylize = true|fun(), -- override with enabled treesitter - vim.lsp.util.stylize_markdown
      cmp_doc = true|fun(), -- reform cmp docs function - require'cmp.entry'.get_documentation
      cmp_sig = true|fun(), -- reform cmp-nvim-lsp-signature-help formatting function to format MD
    },
    ft = { -- only boolean values
      c = true, cpp = true, lua = true, java = true
    },
  },
  input = true|fun()|{},  -- vim.ui.input (used in vim.lsp.buf.rename)
  select = true|fun()|{}, -- vim.ui.select (used in vim.lsp.buf.code_action)
  open_link = true|{      -- keymappings to open uri links (clicked or under cursor)
    {{"", "i"}, "<C-LeftMouse>"},
    {"n", "gl"},
  },
}

-- table of config options for `input` and `select`:
local winConfig = {
  title_pos = "left"|"center"|"right", -- ↓ highlight group of the prompt (replaces `""`)
	title_fmt = {{"[", "FloatBorder"}, {"", "FloatTitle"}, {"]", "FloatBorder"}},
}
```

- **NOTE:** clicking on `file://` links relies on [urlencode](https://github.com/AquilaIrreale/urlencode)
- setup function can be called at any time again to change settings at runtime
