algoritmo "RotinasComRetorno"
procedimento linha_decorativa
inicio
escreval("--------------------------")
fimprocedimento
funcao eh_par(v: inteiro): logico
inicio
se (v - (v \ 2) * 2 = 0) entao
retorne verdadeiro
senao
retorne falso
fimse
fimfuncao
var
num: inteiro
inicio
num <- 14
linha_decorativa
escreval("O número ", num, " é par? ", eh_par(num))
fimalgoritmo
