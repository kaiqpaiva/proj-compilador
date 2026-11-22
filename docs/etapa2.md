# Compilador MiniVisualg — Fase 2: Geração de Código MEPA

**Integrantes:**
- Caio Ariel Cardoso Saraiva — RA 10439611
- Isabela Hissa Pinto — RA 10441873
- Kaique Barros Paiva — RA 10441787

Este documento descreve a parte de **geração de código intermediário (MEPA)** da Fase 2, as decisões de projeto, as limitações conhecidas e o uso de Inteligência Artificial no desenvolvimento.

---

## 1. Como compilar e executar

Compilação (comando do enunciado, sem nenhum warning):

```
gcc -g -Og -Wall compilador.c -o compilador
```

Execução:

```
./compilador testes/validos/15_rotinas.alg
```

Saídas:

| Arquivo | Conteúdo |
|---|---|
| `saida_tokens.txt` | listagem de tokens e resultado da análise (Fase 1) |
| `saida_mepa.txt` | código MEPA gerado (Fase 2) |

Em qualquer erro (léxico, sintático, semântico ou recurso não suportado), o compilador mostra a mensagem com a linha do fonte, encerra e **apaga o `saida_mepa.txt`**, para que nunca sobre um arquivo MEPA incompleto que pareça válido.

### Executando o código gerado

O enunciado não fornece uma máquina virtual MEPA. Para testar, foi criado o interpretador `ferramentas/mepa.py` (ver seção 7):

```
python3 ferramentas/mepa.py saida_mepa.txt                 # executa
python3 ferramentas/mepa.py saida_mepa.txt --trace         # mostra a pilha a cada instrução
python3 ferramentas/mepa.py saida_mepa.txt --entrada 4,7   # valores para os LEIT
```

---

## 2. O que foi implementado

A geração de código é **dirigida pela sintaxe**: o próprio parser descendente recursivo da Fase 1 emite as instruções MEPA enquanto reconhece cada construção, numa única passagem. As variáveis são referenciadas **pelos seus nomes**, como pede o enunciado.

### 2.1 Infraestrutura

- Funções de emissão por formato de operando: `emitir`, `emitirNome`, `emitirInteiro`, `emitirReal`, `emitirDoisInteiros`, `emitirDesvio` e `emitirRotulo`.
- `proximo_rotulo()`, como sugerido no enunciado: devolve 1, 2, 3… a partir de um contador `static`.
- `encerrarComErro()`: centraliza o fechamento dos arquivos e a remoção do MEPA incompleto em todos os erros.

### 2.2 Tradução de cada construção

| Construção | Código MEPA gerado |
|---|---|
| programa | `INPP`, `DSVS` (pula as rotinas), código das rotinas, `AMEM n`, comandos, `PARA` |
| constante inteira / real | `CRCT k` |
| `verdadeiro` / `falso` | `CRCT 1` / `CRCT 0` |
| variável | `CRVL nome` |
| menos unário | operando + `INVR` |
| `+ - *` | `SOMA`, `SUBT`, `MULT` |
| `/` (sempre real no Visualg) | `DIVF` |
| `\` (divisão inteira) | `DIVI` |
| `< <= = > >= <>` | `CMME`, `CMEG`, `CMIG`, `CMMA`, `CMAG`, `CMDG` |
| `e` / `ou` | `CONJ` / `DISJ` |
| atribuição | expressão + `ARMZ nome` |
| `leia(x)` | `LEIT` + `ARMZ x` |
| `escreva` / `escreval` | para cada argumento numérico: expressão + `IMPR` |
| `se ... senao ... fimse` | condição, `DSVF`, então, `DSVS`, rótulo do senão, senão, rótulo de fim |
| `enquanto` | rótulo de início, condição, `DSVF`, corpo, `DSVS` de volta, rótulo de fim |
| `para` | inicialização, rótulo, comparação (`CMEG` ou `CMAG`), `DSVF`, corpo, incremento, `DSVS` |
| procedimento / função | rótulo, `ENPR 1`, corpo, `RTPR 1,0` |
| chamada | argumentos, `ARMZ` em cada parâmetro (do último ao primeiro), `CHPR` |
| chamada de função em expressão | o mesmo + `CRVL nomeDaFuncao` |
| `retorne` | expressão, `ARMZ nomeDaFuncao`, `RTPR 1,0` |

### 2.3 Verificações semânticas feitas nesta parte

Algumas regras são necessárias para que a geração de código seja correta, por isso foram implementadas junto com ela:

- **Regra 6 (parcial):** a **quantidade** de argumentos de uma chamada deve ser igual à de parâmetros da rotina.
- **Regra 7:** procedimento não pode ser usado em expressão.
- **Regra 8:** função não pode ser usada como procedimento.
- `retorne` fora de uma função.
- Identificador usado como rotina sem ter sido declarado como rotina.
- `passo` do `para` igual a zero ou não constante.

A tabela de rotinas já guarda os **tipos** dos parâmetros e o **tipo de retorno**, para que a checagem de tipos (regra 6 completa e regra 9) possa usá-los.

---

## 3. Decisões de projeto

1. **Expressões em notação pós-fixa sem conversão explícita.** O operador é guardado numa variável local antes de consumir o token e só é emitido depois do segundo operando; a recursão do parser já produz a ordem pós-fixa.

2. **Rótulos pedidos no começo de cada comando e guardados em variáveis locais.** Atende ao aviso do enunciado sobre a recursão: um `se` dentro de outro pede os seus próprios rótulos sem sobrescrever os do externo.

3. **`se` sempre pede dois rótulos.** Sem `senao`, o segundo não é usado e a numeração fica com uma lacuna, o que não afeta a execução (cada rótulo continua único).

4. **`DSVS` no início do programa é emitido sempre**, mesmo sem rotinas, para manter o código uniforme. O código das rotinas fica entre o `INPP` e o `AMEM`, e é pulado por esse desvio.

5. **`AMEM` conta posições de memória:** cada variável simples ocupa 1 posição, um vetor ocupa `fim - inicio + 1`, cada parâmetro ocupa 1 e cada função ocupa mais 1 para o valor de retorno.

6. **Textos (`"..."`) não geram código**, porque a MEPA não tem instrução para cadeias. O `escreva` percorre os seus argumentos com um laço próprio e só emite `IMPR` para os que não são texto (por isso não usa a função `argumentos()`, que também serve às chamadas de rotina).

7. **`escreva` e `escreval` geram o mesmo código**, porque a MEPA não tem instrução de quebra de linha.

8. **`passo` do `para` apenas como constante inteira** (`2` ou `-2`). O valor é lido direto do token, sem gerar código, porque no fonte o passo aparece **antes** do corpo, mas o incremento precisa ser emitido **depois** dele. Como o sinal é conhecido na compilação, ele escolhe a comparação: `CMEG` (passo positivo) ou `CMAG` (passo negativo).

9. **Rotinas com parâmetros em posições de memória com nome.** Cada parâmetro é uma posição global, acessada pelo nome (`CRVL n`), e o valor de retorno de uma função fica numa posição com o nome dela. Essa escolha mantém a exigência de referenciar variáveis pelo nome e é segura porque o enunciado exige identificadores **únicos em todo o código** e as rotinas da linguagem não têm variáveis locais. A alternativa (parâmetros na pilha com níveis léxicos e deslocamentos) foi descartada por não usar nomes.

10. **Os argumentos são armazenados do último para o primeiro.** Os argumentos são empilhados na ordem; como o `ARMZ` retira o topo, o primeiro `ARMZ` vai para o último parâmetro.

11. **Tabela de rotinas própria (`Rotina`).** A minitabela `TNo` do enunciado guarda apenas variáveis. As rotinas ficam num vetor fixo (`MAX_ROTINAS`, `MAX_PARAMS`), com verificação de limite.

12. **Recursos não suportados param a compilação com uma mensagem própria** (`NAO SUPORTADO`), em vez de gerar um `saida_mepa.txt` incorreto em silêncio.

---

## 4. Limitações conhecidas

| Limitação | Comportamento |
|---|---|
| **Operador `mod`** | `linha# NAO SUPORTADO: geracao de codigo para o operador mod`. A MEPA não tem instrução de resto, e a fórmula `a - (a \ b) * b` exigiria duplicar valores que já estão na pilha. |
| **Uso de vetores** (`v[i]` em expressão, atribuição e `leia`) | `linha# NAO SUPORTADO: geracao de codigo para vetores`. A MEPA dos slides não tem acesso indexado. A **declaração** de vetores funciona, e o `AMEM` reserva as posições. |
| **Recursão** | Compila, mas não executa corretamente: como os parâmetros são posições globais, uma chamada recursiva sobrescreve os parâmetros da chamada anterior. |
| **Textos e variáveis `caractere`** | Textos no `escreva` são ignorados na geração; `LEIT` e `IMPR` trabalham apenas com números. |
| **Quebra de linha do `escreval`** | Não representável na MEPA (igual ao `escreva`). |
| **`passo` não constante** | Erro semântico. |
| **Instruções de rotina** | `CHPR`, `ENPR` e `RTPR` não aparecem nos slides da disciplina; foram usadas no formato do livro de onde a MEPA se origina (ver seção 8). |

Por causa das duas primeiras limitações, os testes da Fase 1 `05_repeticao_vetor.alg` (vetores) e `06_procedimento_funcao.alg` (`mod`) continuam válidos para as análises léxica e sintática, mas param na geração de código com a mensagem de "não suportado".

---

## 5. Integração com as demais partes da Fase 2

Partes da análise semântica sob responsabilidade dos outros integrantes, que se integram com a geração:

- minitabela de símbolos `TNo` em lista encadeada e `busca_tabela_simbolos()`;
- variável declarada antes do uso, palavra reservada como nome, identificador único e de tipo fixo, variável declarada e atribuída que não é usada;
- tipos compatíveis em expressões e **tipos** dos argumentos e do retorno (a tabela de rotinas já guarda essas informações).

---

## 6. Testes

### Válidos (geração de código)

| Teste | O que verifica | Saída esperada no `mepa.py` |
|---|---|---|
| `07_amem_varias_secoes` | `AMEM` com várias seções `var` e vetores | `AMEM 14` |
| `08_amem_parametros` | parâmetros e retorno entram no `AMEM` | `AMEM 4` |
| `09_expressoes` | precedência, associatividade, unário, divisões e lógicos | variáveis: `x1=14 x2=20 x3=3 x4=-5 x5=3 r1=3.5 r2=3.0 l1=0 l2=1` |
| `10_leia` | `LEIT` + `ARMZ` | com `--entrada 6,7`: `a=42 b=7` |
| `11_escreva` | `IMPR` só para valores numéricos | `20 40 10.0 21` |
| `12_se` | `se` com e sem `senao`, aninhado | `1 3 5` |
| `13_enquanto` | `enquanto` simples, aninhado e que não executa | `15 1 1 2 1 2 3` |
| `14_para` | `para` com passo padrão, negativo e aninhado | `1 2 3 10 6 2 11 12 21 22` |
| `15_rotinas` | procedimento sem e com parâmetros, função, chamada aninhada, `retorne` no meio | `0 7 34 9 9 0` |

### Erros (pasta `testes/erros/`)

| Teste | Mensagem |
|---|---|
| `semantico_passo_zero` | `passo do para nao pode ser zero` |
| `semantico_passo_variavel` | `passo do para deve ser uma constante inteira` |
| `semantico_retorne_fora_funcao` | `retorne so pode ser usado dentro de uma funcao` |
| `semantico_funcao_como_procedimento` | `funcao nao pode ser usada como procedimento` |
| `semantico_procedimento_em_expressao` | `procedimento nao pode ser usado em expressao` |
| `semantico_numero_argumentos` | `quantidade de argumentos incompativel com a declaracao da rotina` |

Em todos os testes de erro, o `saida_mepa.txt` não existe ao final da execução.

---

## 7. Execução do código gerado e a ferramenta `ferramentas/mepa.py`

### 7.1 Por que foi preciso uma máquina virtual

O enunciado pede que o arquivo TXT gerado "seja executado na máquina virtual MEPA", mas nenhuma máquina virtual foi fornecida junto com o projeto. Sem uma forma de executar o código, a única verificação possível seria conferir o `saida_mepa.txt` à mão, linha por linha, o que não é viável para programas com laços aninhados e rotinas.

### 7.2 A VM encontrada: `maquina-virtual-mepa`, de HugoJF

Procurando no GitHub por uma implementação pública da MEPA, encontramos o repositório **`HugoJF/maquina-virtual-mepa`** (https://github.com/HugoJF/maquina-virtual-mepa), com uma versão executável no navegador em https://hugojf.github.io/maquina-virtual-mepa/mepa.html. É um projeto independente, de terceiros, feito em JavaScript. Segundo o próprio README do projeto, a implementação foi feita a partir de trechos de um livro, sem identificar a obra. Pelo conjunto e pelo formato das instruções (por exemplo, `CRVL k,n` com nível léxico e deslocamento), trata-se da MEPA do livro de Tomasz Kowaltowski (ver seção 8.3).

Testamos essa VM com programas escritos à mão e encontramos estas limitações:

- **Formato diferente do enunciado:** não aceita variáveis pelo nome (`CRVL a`), apenas o formato com nível e deslocamento (`CRVL 0,2`).
- **`MULT` grafado como `MULTI`:** a multiplicação no formato padrão não é reconhecida.
- **Não tem `DIVF`** (divisão real).
- **`LEIT` não lê do teclado:** empilha um número aleatório entre 0 e 10.
- **Não permite acompanhar a execução:** o botão de executar uma instrução por vez não atualiza a tela, e as pilhas nunca são exibidas; o rastro só aparece no console do navegador.

Ou seja, para usá-la seria preciso converter manualmente cada `saida_mepa.txt` (nomes para `0,endereco`, `MULT` para `MULTI`), e ainda assim não seria possível testar leitura de dados nem divisão real.

### 7.3 A ferramenta `ferramentas/mepa.py`

Por isso foi escrito um interpretador MEPA próprio, em Python 3 e sem dependências, usado **apenas para testar** o código gerado; ele **não faz parte do compilador**. Ele:

- lê diretamente o formato gerado pelo compilador (variáveis por nome e rótulos `Ln: NADA`), sem nenhuma conversão;
- executa todas as instruções usadas, inclusive `DIVF` e as de rotina (`CHPR`, `ENPR`, `RTPR`), e aceita também o formato da VM de HugoJF (`CRVL 0,2`, `MULTI`), para comparação;
- lê de verdade os valores do `LEIT`, pelo teclado ou pela opção `--entrada`;
- mostra a pilha e as variáveis depois de cada instrução com `--trace`;
- acusa com a linha do arquivo os erros que indicariam código gerado errado: rótulo não definido, pilha vazia, `CRCT` com nome de variável, variável fora da área reservada pelo `AMEM`, `RTPR` fora de uma rotina, divisão por zero e laço infinito.

---

## 8. Uso de Inteligência Artificial

Conforme exigido pelo enunciado, informamos que a ferramenta **Claude (Anthropic)** foi usada no desenvolvimento da parte de geração de código. O uso se deu principalmente como **tutor e revisor**: a maior parte do código foi escrita pelo integrante e depois revisada pela IA. Todo o código foi lido, testado e compreendido antes de ser incorporado.

### 8.1 Como a IA foi usada

- **Estudo do conteúdo:** explicação de análise semântica, código intermediário e MEPA, com exercícios.
- **Planejamento:** leitura crítica do enunciado e do código da Fase 1, divisão em etapas (infraestrutura, moldura, expressões, entrada e saída, controle, rotinas) e identificação de riscos (o formato `TNo`, a ausência de VM, `mod` e vetores).
- **Decisões de projeto:** discussão das alternativas e dos trade-offs de cada decisão da seção 3, sempre com a decisão final tomada pelo integrante.
- **Revisão de código:** cada função escrita foi revisada, com a indicação de erros de compilação, warnings do `-Wall` e erros de lógica.
- **Testes:** sugestão dos programas de teste e dos resultados esperados, e conferência das execuções.

### 8.2 Trechos mais críticos com participação direta da IA

- **Ordem de emissão nas expressões:** a necessidade de guardar o operador, o `op_code`, o valor numérico e o índice do identificador **antes** de consumir o token foi apontada pela IA; o código foi escrito pelo integrante e revisado.
- **`cmd_para`:** a decisão de ler o passo direto do token (o passo aparece antes do corpo no fonte, mas o incremento precisa vir depois) e a escolha entre `CMEG` e `CMAG` pelo sinal foram propostas pela IA.
- **Rotinas:** a estratégia de parâmetros como posições de memória com nome, o retorno guardado no nome da função, a ordem reversa dos `ARMZ` na chamada e a estrutura `Rotina` foram propostos pela IA.
- **Correções apontadas pela IA:** contagem de identificadores em `lista_ids` (contava um a menos), declaração logo após um `case` (não compila em gcc anterior à versão 11), ausência de `default` em `switch` sobre enum (gera warning com `-Wall`), retorno do `fread` ignorado (warning com o comando do enunciado), formato do `emitirDoisInteiros` e posição da leitura do tipo de retorno na `funcao`.
- **`naoSuportado` e os pontos de verificação de vetores e `mod`:** fornecidos pela IA.
- **`ferramentas/mepa.py`:** escrito integralmente pela IA, como ferramenta de teste.

### 8.3 Instruções de rotina (`CHPR`, `ENPR`, `RTPR`)

**O problema.** O enunciado pede a geração de código para "todos os comandos e rotinas da linguagem", mas os slides da disciplina sobre a MEPA cobrem apenas expressões, atribuição, desvios, leitura e escrita: não há nenhuma instrução para chamar um procedimento ou uma função, nem para voltar dela.

**A origem da MEPA.** A MEPA (Máquina de Execução para Pascal) foi definida no livro *Implementação de Linguagens de Programação*, de Tomasz Kowaltowski (1983), uma referência clássica no ensino de compiladores no Brasil. O livro define, além das instruções que aparecem nos slides, um conjunto para rotinas:

| Instrução | Função |
|---|---|
| `CHPR p` | chama a rotina que começa no rótulo `p`: empilha o endereço de retorno e desvia |
| `ENPR k` | entrada da rotina de nível léxico `k`: salva o registrador de base `D[k]` e o atualiza |
| `RTPR k,n` | retorno: restaura `D[k]`, volta ao endereço guardado pelo `CHPR` e descarta `n` parâmetros da pilha |

**Como as instruções foram obtidas.** A IA identificou essas instruções lendo o código-fonte da VM de HugoJF (seção 7.2), que as implementa no formato do livro, e reconheceu nelas a MEPA de Kowaltowski. **A IA não teve acesso ao livro**: a semântica descrita acima foi tirada da implementação de HugoJF, e não do texto original. O mesmo comportamento foi reproduzido no `mepa.py`, e o compilador passou a emitir essas instruções, preservando os nomes e o formato do livro para manter a compatibilidade com outras implementações da MEPA.

**Como foram usadas aqui.** No livro, os parâmetros ficam na pilha e são acessados por nível léxico e deslocamento. Nesta implementação, os parâmetros ficam em posições de memória com nome (decisão 9 da seção 3), para respeitar a exigência do enunciado de referenciar variáveis pelo nome. Por isso, nenhum parâmetro é colocado na pilha, e o compilador sempre emite `ENPR 1` (todas as rotinas estão no nível 1, abaixo do programa principal) e `RTPR 1,0` (zero parâmetros a descartar).
