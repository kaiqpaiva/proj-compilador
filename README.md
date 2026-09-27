# Compiladores – Projeto Fase 1: MiniVisualg

Analisador léxico e sintático da linguagem MiniVisualg, um subconjunto do Visualg definido pelos exemplos do Anexo I do enunciado.

## Integrantes

| Nome | RA |
|---|---|
| Caio Ariel Cardoso Saraiva | 10439611 |
| Isabela Hissa Pinto | 10441873 |
| Kaique Barros Paiva | 10441787 |

## 1. O que foi concluído

- [x] Etapa 1 – Expressões regulares e gramática ([docs/etapa1_gramatica.md](docs/etapa1_gramatica.md))
- [x] Etapa 2 – Analisador léxico
- [x] Etapa 3 – Analisador sintático

## 2. Como compilar e executar

```sh
gcc -Wall -Wno-unused-result -g -Og compilador.c -o compilador
./compilador testes/validos/01_primeiro_passo.alg
```

Os tokens aparecem na tela e são salvos em `saida_tokens.txt`, no formato `linha# NOME | atributo`.

Ao final aparece `Analise sintatica concluida sem erros.` ou a mensagem de `ERRO LEXICO` / `ERRO SINTATICO` com a linha do erro.

### Testes

Para rodar todos os testes de uma vez:

```sh
for f in testes/*/*.alg; do echo "== $f"; ./compilador $f | tail -1; done
```

| Arquivo | Resultado esperado |
|---|---|
| `testes/validos/01` a `06` | `Analise sintatica concluida sem erros.` |
| `testes/erros/lexico_simbolo_invalido.alg` | `5# ERRO LEXICO: '@'` |
| `testes/erros/lexico_string_aberta.alg` | `4# ERRO LEXICO: '"essa string nunca fecha)'` |
| `testes/erros/sintatico_falta_entao.alg` | `7# ERRO SINTATICO: esperado ENTAO, encontrado ESCREVAL` |
| `testes/erros/sintatico_falta_fimpara.alg` | `7# ERRO SINTATICO: esperado FIMPARA, encontrado FIMALGORITMO` |

## 3. Decisões de design

### Geral

- Tudo fica em um único `compilador.c`, porque o comando de compilação do enunciado compila só esse arquivo.

### Analisador léxico

- **Tokens de palavras reservadas:** criamos um token para cada palavra reservada (`TOKEN_SE`, `TOKEN_ENTAO`...) em vez de um `TOKEN_KEYWORD` genérico, para facilitar o parser.
- **Operador `<>`:** adicionamos `OP_NE` para esse operador, que aparece no anexo mas não está no enum do enunciado. A igualdade no Visualg é `=`, não `==`.
- **Maiúsculas e minúsculas:** as palavras reservadas são reconhecidas sem diferenciar maiúsculas de minúsculas, porque o anexo usa `MOD` e `E` em maiúsculo.
- **Separação por espaço:** o enunciado diz que os lexemas estão separados por espaço, mas os exemplos do anexo não seguem isso (ex: `escreval("...")`). O léxico foi feito para funcionar mesmo sem espaços.
- **Registro dos tokens:** o léxico registra cada token dentro de `obterToken`, que é uma casca fina sobre `reconhecerToken`. Assim a listagem sai numa passagem só, conforme a Figura 1 do enunciado, com o parser dirigindo o léxico. Isso só é seguro porque a gramática é LL(1) e nenhum token é lido duas vezes.
- **Buffer do `infoToken`:** `infoToken` monta a linha de saída num buffer fornecido pelo chamador, em vez de um buffer `static` interno. Assim, duas chamadas no mesmo `printf` não sobrescrevem uma à outra, desde que cada uma receba o seu próprio buffer.
- **Cadeias com aspas:** as cadeias são guardadas na tabela de símbolos **com** as aspas, para que a cadeia `"idade"` e a variável `idade` não ocupem o mesmo índice.
- **Lexemas longos:** um lexema com mais de 255 caracteres gera `ERRO LEXICO` em vez de ser truncado em silêncio. Truncar faria dois identificadores longos e diferentes virarem o mesmo símbolo.
- **Faixa `1..4`:** em `1..4`, o léxico devolve `NUM_INT` e deixa o `..` para a chamada seguinte. Um ponto seguido de algo que não é dígito nem ponto (ex: `1.`) é um real malformado e gera erro léxico.
- **Tabela de nomes dos tokens:** é um array indexado pelo enum, com um sentinela `TOKEN_TOTAL` e um `_Static_assert` que quebra a compilação se as duas listas ficarem dessincronizadas.
- **Fechamento do arquivo no erro léxico:** `erroLexico` grava a mensagem na tela e no arquivo, e fecha o arquivo antes do `exit(1)`. Sem o `fclose`, os tokens já escritos ficariam no buffer do stdio, e o arquivo sairia vazio justamente nos testes de erro.

### Analisador sintático

- **Tipo de parser:** o analisador sintático é **descendente recursivo preditivo (LL(1))**. Há uma função por não-terminal da gramática da Etapa 1, e a produção é escolhida só pelo token atual (`lookahead`), sem retrocesso.
- **Uma passagem só:** o parser dirige o léxico. Cada token é pedido com `nextToken()` e impresso na hora, então a listagem e a análise saem numa passagem só.
- **Mensagem de erro:** a mensagem de erro sintático mostra o token esperado e o encontrado, e também é gravada em `saida_tokens.txt`.
- **Fim do arquivo:** no fim, o `main` só confere que o token atual é `EOF`, sem consumi-lo, para não pedir um token depois do fim do arquivo.

## 4. Bugs conhecidos

- **Tabela de símbolos cheia:** `inserirSimbolo` não verifica se a tabela de símbolos encheu. Acima de `MAX_SIMBOLOS` (512) símbolos distintos, há escrita fora dos limites do array. Nenhum arquivo de teste chega perto disso, mas a checagem está pendente.
- **Identificadores com acento:** não são suportados. O alfabeto da Etapa 1 define letras como `a-zA-Z`, então `media` funciona e `média` é cortada no acento. Isso gera um erro léxico cuja sequência exibida sai ilegível (byte UTF-8 solto). Acentos dentro de cadeias e comentários funcionam normalmente.
- **Inteiros muito grandes:** um literal inteiro maior que o limite de `int` é convertido por `strtol` com saturação, sem aviso. Não há verificação de overflow.
- **`TOKEN_KEYWORD` não usado:** continua no enum por fidelidade à Figura 2 do enunciado, mas não é usado, porque cada palavra reservada tem seu próprio token.