COMPILADORES - PROJETO FASE 1 - MiniVisualg
Integrantes:
  Caio Ariel Cardoso Saraiva  - RA 10439611
  Isabela Hissa Pinto         - RA 10441873
  Kaique Barros Paiva         - RA 10441787

1. O QUE FOI CONCLUIDO
  [x] Etapa 1 - Expressoes regulares e gramatica (docs/etapa1_gramatica.md)
  [X] Etapa 2 - Analisador lexico
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
  - O lexico registra cada token dentro do obterToken, que e uma casca
    fina sobre reconhecerToken. Assim a listagem sai numa passagem so,
    conforme a Figura 1 do enunciado, e continuara saindo na Etapa 3
    quando for o parser a dirigir o lexico. Isso so e seguro porque a
    gramatica e LL(1) e nenhum token e lido duas vezes.
  - infoToken monta a linha de saida num buffer fornecido pelo chamador,
    em vez de um buffer static interno. Duas chamadas no mesmo printf
    ainda exigem buffers distintos.
  - Cadeias sao guardadas na tabela de simbolos COM as aspas, para que
    a cadeia "idade" e a variavel idade nao ocupem o mesmo indice.
  - Lexema com mais de 255 caracteres gera ERRO LEXICO em vez de ser
    truncado em silencio: truncar faria dois identificadores longos e
    diferentes virarem o mesmo simbolo.
  - Em "1..4" o lexico devolve NUM_INT e deixa o ".." para a chamada
    seguinte. Um ponto seguido de nao-digito e nao-ponto (ex: "1.") e
    real malformado e gera erro lexico.
  - A tabela de nomes dos tokens e um array indexado pelo enum, com um
    sentinela TOKEN_TOTAL e um _Static_assert que quebra a compilacao se
    as duas listas ficarem dessincronizadas.
  - erroLexico grava a mensagem na tela e no arquivo, e fecha o arquivo
    antes do exit(1). Sem o fclose os tokens ja escritos ficariam no
    buffer do stdio e o arquivo sairia vazio justamente nos testes de
    erro.

4. BUGS CONHECIDOS
  - inserirSimbolo nao verifica se a tabela de simbolos encheu. Acima de
    MAX_SIMBOLOS (512) simbolos distintos ha escrita fora dos limites do
    array. Nenhum arquivo de teste chega perto disso, mas a checagem
    esta pendente.
  - Identificadores com acento nao sao suportados. O alfabeto da Etapa 1
    define letras como a-zA-Z, entao "media" funciona e "média" e
    cortada no acento, gerando um erro lexico cuja sequencia exibida sai
    ilegivel (byte UTF-8 solto). Acentos dentro de cadeias e comentarios
    funcionam normalmente.
  - Literal inteiro maior que o limite de int e convertido por strtol com
    saturacao, sem aviso. Nao ha verificacao de overflow.
  - TOKEN_KEYWORD continua no enum por fidelidade a Figura 2 do
    enunciado, mas nao e usado: cada palavra reservada tem seu proprio
    token.

