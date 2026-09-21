COMPILADORES - PROJETO FASE 1 - MiniVisualg
Integrantes:
  Caio Ariel Cardoso Saraiva  - RA 10439611
  Isabela Hissa Pinto         - RA 10441873
  Kaique Barros Paiva         - RA 10441787

1. O QUE FOI CONCLUIDO
  [x] Etapa 1 - Expressoes regulares e gramatica (docs/etapa1_gramatica.md)
  [ ] Etapa 2 - Analisador lexico
  [ ] Etapa 3 - Analisador sintatico

2. COMO COMPILAR E EXECUTAR
  gcc -Wall -Wno-unused-result -g -Og compilador.c -o compilador
  ./compilador testes/validos/01_primeiro_passo.alg

  Os tokens aparecem na tela e sao salvos em saida_tokens.txt

3. DECISOES DE DESIGN
  - Tudo fica em um unico compilador.c porque o comando de compilacao
    do enunciado compila so esse arquivo.
  - Criamos um token para cada palavra reservada (TOKEN_SE, TOKEN_ENTAO...)
    em vez de um TOKEN_KEYWORD generico, pra facilitar o parser.
  - Adicionamos OP_NE para o operador <>, que aparece no anexo mas nao
    esta no enum do enunciado. Igualdade no Visualg e "=", nao "==".
  - Palavras reservadas sao reconhecidas sem diferenciar maiusculas
    (o anexo usa MOD e E em maiusculo).
  - O enunciado diz que os lexemas estao separados por espaco, mas os
    exemplos do anexo nao seguem isso (ex: escreval("...")). O lexico
    foi feito para funcionar mesmo sem espacos.

4. BUGS CONHECIDOS
  (preencher)
