" Name: ts.vim
" Section: Documentation {{{1
" Copyright: Google Inc.
" License: Apache 2.0
" Description:
"   
"   This script implements transparent editing of mpeg2ts (.ts) files. The
"   The filename must have a ".ts" suffix. When opening such a file
"   the content is transformed into text (using m2pb). The contents
"   are transformed back into a ts file format before writing.
"
" Installation: 
"
"   Copy the ts.vim file to your $HOME/.vim/plugin directory.
"   Refer to ':help add-plugin', ':help add-global-plugin' and ':help
"   runtimepath' for more details about Vim plugins.

" Nice editing of ts traces.
" Inspired on http://www.vi-improved.org/wiki/index.php/VimGpg
augroup tstrace
	autocmd!
	" Ensure everybody knows this is ts (i.e., no bin syntax)
	autocmd BufNewFile,BufRead          *.ts set filetype=ts
	" First make sure nothing is written to ~/.viminfo while editing
	" an encrypted file.
	autocmd BufReadPre,FileReadPre      *.ts set viminfo=
	" Switch to binary mode to read the trace
	autocmd BufReadPre,FileReadPre      *.ts set bin
	autocmd BufReadPre,FileReadPre      *.ts let shsave=&sh
	autocmd BufReadPre,FileReadPre      *.ts let &sh='sh'
	autocmd BufReadPre,FileReadPre      *.ts let ch_save = &ch|set ch=2
	"autocmd BufReadPost,FileReadPost    *.ts '[,']!tee DEBUG
	autocmd BufReadPost,FileReadPost    *.ts '[,']!m2pb totxt 2> /dev/null
	"autocmd BufReadPost,FileReadPost    *.ts '[,']!m2pb totxt 2> /tmp/vimlog
	autocmd BufReadPost,FileReadPost    *.ts let &sh=shsave
	" Switch to normal mode for editing
	autocmd BufReadPost,FileReadPost    *.ts set nobin
	autocmd BufReadPost,FileReadPost    *.ts let &ch = ch_save|unlet ch_save
	autocmd BufReadPost,FileReadPost    *.ts execute ":doautocmd BufReadPost " . expand("%:r")
	" Convert all text to trace before writing
	autocmd BufWritePre,FileWritePre    *.ts set bin
	autocmd BufWritePre,FileWritePre    *.ts let shsave=&sh
	autocmd BufWritePre,FileWritePre    *.ts let &sh='sh'
	" http://tech.groups.yahoo.com/group/vim/message/78100
	autocmd BufWritePre,FileWritePre    *.ts set noendofline
	autocmd BufWritePre,FileWritePre    *.ts '[,']!m2pb tobin 2>/dev/null
	autocmd BufWritePre,FileWritePre    *.ts let &sh=shsave
	" Undo the ascii->ts so we are back in the normal text, directly
	" after the file has been written.
	autocmd BufWritePost,FileWritePost  *.ts silent u
	autocmd BufWritePost,FileWritePost  *.ts set nobin
augroup END


