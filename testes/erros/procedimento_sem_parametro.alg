algoritmo "ProcedimentosSemParametros"

procedimento linha_decorativa
inicio
escreval("-------------------------------")
fimdeprocedimento

algoritmo "ProcedimentosComParametros""
inicio
escreval("Cabeçalho do Sistema")
linha_decorativa
escreval("Bem-vindo!")
linha_decorativa
fimalgoritmo

procedimento mostrar_erro(mensagem: caractere)
inicio
escreval("[ERRO DO SISTEMA]: ", mensagem)
fimprocedimento

inicio
mostrar_erro("A conexão com a internet caiu.")
    