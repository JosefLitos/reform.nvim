---@type reform.util
---@diagnostic disable-next-line: missing-fields
local M = {
	winConfig = {
		title_pos = 'center',
		title = { { '[', 'FloatBorder' }, { '', 'FloatTitle' }, { ']', 'FloatBorder' } },
		relative = 'cursor',
		border = 'rounded',
	},
	filter = {
		tolerance = { startPost = 0, startPre = math.huge, endPost = math.huge, endPre = 0 },
		sorting = { order = 1, matcher = 0, offset = 1, length = 0 },
	},
	debug = false,
}
function M.mkWin(buf, opts, prompt)
	local mode = vim.api.nvim_get_mode().mode
	vim.bo[buf].buftype = 'nofile'
	vim.bo[buf].bufhidden = 'wipe'
	if opts.height < 1 then opts.height = 1 end
	if opts.width < 1 then opts.width = 1 end
	opts.title[2][1] = prompt ~= '' and prompt or '⋯'
	local winhl = opts.winhl or ''
	opts.winhl = nil
	local win = vim.api.nvim_open_win(buf, true, opts)
	opts.winhl = winhl
	vim.wo[win].winhighlight = 'Search:NONE,Pmenu:Normal,MatchParen:NONE,' .. winhl
	vim.wo[win].number = false
	vim.wo[win].relativenumber = false
	vim.wo[win].cursorcolumn = false
	vim.api.nvim_create_autocmd('BufLeave', {
		buffer = buf,
		callback = function()
			if mode == 'i' then
				vim.cmd.startinsert()
			else
				vim.cmd.stopinsert()
			end
			vim.api.nvim_win_close(win, true)
		end,
	})

	return win
end

function M.findMatch(event, matchers, default, filter)
	filter = vim.tbl_deep_extend('force', M.filter, filter, event.filter or {})
	local line = vim.api.nvim_buf_get_lines(event.buf, event.line - 1, event.line, true)[1]
	local column = event.column

	local function formatFind(from, to, ...) return { from = from, to = to, ... } end
	local function iter(matcher)
		local ret
		if matcher.luapat then
			local pat = matcher.luapat
			ret = function(_, from)
				local ret = formatFind(line:find(pat, from))
				if ret[1] then return ret.to + 1, ret end
			end
		else
			local re = vim.regex(matcher.vimre)
			ret = function(_, _from)
				local from, to = re:match_str(line:sub(_from))
				if not from then return end
				from, to = from + _from, to + _from - 1
				local ret = vim.fn.matchlist(line:sub(from, to), matcher.vimre) -- includes entire match
				ret.from = from
				ret.to = to
				return to + 1, ret
			end
		end
		return ret, nil, 1
	end

	local sorter = filter.sorting
	if type(sorter) == 'table' then
		local sorting = sorter
		sorter = function(order, matcher, match)
			return sorting.order * order
				+ (
					(match.from <= column and match.to >= column) and -1000
					or (
						sorting.matcher * (matcher.group or 10)
						+ sorting.offset * math.abs(match.from - column)
						+ sorting.length * #match[1]
					)
				)
		end
	end
	local order = {}
	local startPre = filter.tolerance.startPre
	local startPost = filter.tolerance.startPost
	local endPre = filter.tolerance.endPre
	local endPost = filter.tolerance.endPost

	if type(matchers) == 'function' then matchers = matchers(event) end
	for i, matcher in ipairs(matchers) do
		if type(matcher) == 'string' then matcher = default[matcher] end
		for _, match in iter(matcher) do
			local from, to = match.from, match.to
			if
				math.abs(from - column) <= ((from <= column) and startPre or startPost)
				and math.abs(to - column) <= ((to >= column) and endPost or endPre)
			then
				order[#order + 1] = { sorter(i, matcher, match), matcher, match }
			end
		end
	end

	table.sort(order, function(a, b) return a[1] < b[1] end)
	if M.debug then
		local data = {}
		for i, v in ipairs(order) do
			data[i] = { v[1], v[3][1], v[3].from, v[3].to }
		end
		vim.notify('reform.util.findMatch.order: ' .. vim.inspect(data))
	end
	for _, pair in ipairs(order) do
		if pair[2].use(pair[3][1], pair[3], event) ~= false then return pair[3] end
	end

	return false -- no successful matcher found
end

function M.applyMatcher(matcher, ev)
	ev = ev or {}
	local pos = vim.api.nvim_win_get_cursor(0)
	ev.line = pos[1]
	ev.column = pos[2] + 1
	ev.buf = 0
	return M.findMatch(ev, { matcher }, {}, {})
end

return M
