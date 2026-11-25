algoritmo "ProcedFunc"
procedimento linha
inicio
   escreval(0)
fimprocedimento
procedimento mostra(v: inteiro)
inicio
   escreval(v)
fimprocedimento
funcao soma(a: inteiro, b: inteiro): inteiro
inicio
   retorne a + b
fimfuncao
funcao maior(p: inteiro, q: inteiro): inteiro
inicio
   se p > q entao
      retorne p
   fimse
   retorne q
fimfuncao
var
x: inteiro
inicio
linha
x <- soma(3, 4)
mostra(x)
mostra(soma(x, 10) * 2)
escreval(maior(5, 9))
escreval(maior(9, 5))
linha
fimalgoritmo
