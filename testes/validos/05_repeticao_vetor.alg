algoritmo "MediaNotas"
var
notas: vetor[1..4] de real
soma: real
i, contador: inteiro
inicio
soma <- 0
para i de 1 ate 4 faca
escreva("Digite a nota do aluno ", i, ": ")
leia(notas[i])
soma <- soma + notas[i]
fimpara
para i de 10 ate 0 passo -2 faca
escreval(i)
fimpara
contador <- 1
enquanto (contador <= 5) faca
contador <- contador + 1
fimenquanto
escreval("A média da turma foi: ", soma / 4)
fimalgoritmo
