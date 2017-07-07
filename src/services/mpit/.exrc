if &cp | set nocp | endif
noremap  :tabp
let s:cpo_save=&cpo
set cpo&vim
nmap gx <Plug>NetrwBrowseX
nnoremap <silent> <Plug>NetrwBrowseX :call netrw#NetrwBrowseX(expand("<cWORD>"),0)
let &cpo=s:cpo_save
unlet s:cpo_save
set backspace=2
set cscopeprg=/usr/bin/cscope
set cscopetag
set cscopeverbose
set guicursor=n-v-c:block,o:hor50,i-ci:hor15,r-cr:hor30,sm:block,a:blinkon0
set hlsearch
set ruler
" vim: set ft=vim :
