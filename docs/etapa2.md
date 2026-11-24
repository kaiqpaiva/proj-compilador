# Compilador MiniVisualg — Fase 2: Análise Semântica e Geração de Código MEPA

**Integrantes:**
- Caio Ariel Cardoso Saraiva — RA 10439611
- Isabela Hissa Pinto — RA 10441873
- Kaique Barros Paiva — RA 10441787

Este documento descreve a Fase 2: a geração de código MEPA, as verificações semânticas e como as duas se integram, além das limitações, dos testes e do uso de IA. As decisões de projeto da análise semântica estão no [README](../README.md), seção 3.

---

## 1. Como compilar e executar

```
gcc -g -Og -Wall compilador.c -o compilador
./compilador testes/validos/15_rotinas.alg
```

A listagem de tokens e o resultado vão para `saida_tokens.txt`, e o código MEPA para `saida_mepa.txt`. Em qualquer erro, o compilador mostra a mensagem com a linha, encerra e **apaga o `saida_mepa.txt`**, para que nunca sobre um arquivo incompleto que pareça válido.

Para executar o código gerado, use o interpretador `ferramentas/mepa.py` (seção 7):

```
python3 ferramentas/mepa.py saida_mepa.txt                 # executa
python3 ferramentas/mepa.py saida_mepa.txt --trace         # mostra a pilha a cada instrução
python3 ferramentas/mepa.py saida_mepa.txt --entrada 4,7   # valores para os LEIT
```

---

## 2. O que foi implementado

A geração de código é **dirigida pela sintaxe**: o parser descendente recursivo da Fase 1 emite as instruções MEPA enquanto reconhece cada construção, numa única passagem. As variáveis são referenciadas **pelos nomes**, como pede o enunciado.

### 2.1 Infraestrutura

- Uma função de emissão por formato de operando: `imprimir`, `imprimir_nome`, `imprimir_inteiro`, `imprimir_real`, `imprimir_dois_inteiros`, `imprimir_desvio` e `imprimir_rotulo`.
- `proximo_rotulo()`, como sugerido no enunciado: devolve 1, 2, 3… a partir de um contador `static`.
- `sair_com_erro()`: fecha os arquivos e apaga o MEPA incompleto em todos os erros.

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

### 2.3 Verificações semânticas

As 9 regras do enunciado estão implementadas, e todas encerram a compilação com `linha# ERRO SEMANTICO: <motivo>`.

| Regra do enunciado | Onde é verificada |
|---|---|
| 1. Variável declarada antes do uso | `busca_tabela_simbolos`, em todo uso de variável (`fator`, `cmd_id`, `variavel` e `cmd_para`) |
| 2. Palavra reservada não pode ser nome de variável | `registrar_id`, `parametro` e `secao_var` |
| 3. Identificador único e com tipo fixo | `inserir_var_semant`, `registrar_var`, `procedimento` e `funcao` |
| 4. Variável declarada e atribuída deve ser usada | `conferir_variaveis_nao_usadas`, no fim de `programa` |
| 5. Tipos compatíveis nas expressões | as funções de expressão devolvem o tipo e cada operador confere os dois lados; também a atribuição, as condições e o `para` |
| 6. Tipo e número dos argumentos | `gerar_chamada`: primeiro a quantidade, depois o tipo de cada argumento |
| 7. Procedimento não pode ser usado em expressão | `fator` |
| 8. Função não pode ser usada como procedimento | `cmd_id` |
| 9. Retorno compatível com a declaração | `cmd_retorne` (tipo do valor) e `funcao` (precisa ter algum `retorne`) |

O compilador também dá erro semântico para `retorne` fora de uma função, identificador usado como rotina sem ter sido declarado como rotina e `passo` do `para` igual a zero ou não constante.

---

## 3. Decisões de projeto da geração de código

1. **Expressões em ordem pós-fixa sem conversão explícita.** O operador é guardado antes de consumir o token e só é emitido depois do segundo operando; a recursão do parser já produz a ordem pós-fixa.
2. **Rótulos pedidos no começo de cada comando e guardados em variáveis locais**, como alerta o enunciado: um `se` dentro de outro pede os próprios rótulos sem sobrescrever os do externo. O `se` sempre pede dois; sem `senao`, o segundo fica sem uso, o que não afeta a execução.
3. **`DSVS` no início do programa sempre**, mesmo sem rotinas: o código das rotinas fica entre o `INPP` e o `AMEM` e é pulado por esse desvio.
4. **`AMEM` conta posições de memória:** 1 por variável simples, `fim - inicio + 1` por vetor, 1 por parâmetro e mais 1 por função, para o valor de retorno.
5. **Textos não geram código**, porque a MEPA não tem instrução para cadeias. O `escreva` só emite `IMPR` para os argumentos que não são texto (por isso tem um laço próprio, em vez de usar `argumentos()`). Fora do `escreva`, um texto é recurso não suportado (seções 4 e 5).
6. **`escreva` e `escreval` geram o mesmo código**, porque a MEPA não tem quebra de linha.
7. **`passo` do `para` só como constante inteira.** No fonte o passo vem **antes** do corpo, mas o incremento precisa ser emitido **depois** dele; por isso o valor é lido direto do token. O sinal escolhe a comparação: `CMEG` (positivo) ou `CMAG` (negativo).
8. **Parâmetros em posições de memória com nome.** Cada parâmetro é uma posição global acessada pelo nome (`CRVL n`), e o retorno de uma função fica numa posição com o nome dela. Isso mantém a exigência de referenciar variáveis pelo nome e funciona porque os identificadores são únicos em todo o código e as rotinas não têm variáveis locais. Parâmetros na pilha, com níveis léxicos, foram descartados por não usarem nomes.
9. **Argumentos armazenados do último para o primeiro**, porque o `ARMZ` retira o topo da pilha, onde está o último argumento.
10. **Recursos não suportados param a compilação** com `NAO SUPORTADO`, em vez de gerar um `saida_mepa.txt` incorreto em silêncio.

---

## 4. Limitações conhecidas

| Limitação | Comportamento |
|---|---|
| **Operador `mod`** | `linha# NAO SUPORTADO: geracao de codigo para o operador mod`. A MEPA não tem instrução de resto, e a fórmula `a - (a \ b) * b` usa cada operando duas vezes, mas a MEPA não tem instrução para duplicar o topo da pilha. |
| **Vetores** (`v[i]` em expressão, atribuição e `leia`) | `linha# NAO SUPORTADO: geracao de codigo para vetores`. A MEPA dos slides não tem acesso indexado. A **declaração** funciona, e o `AMEM` reserva as posições. |
| **Textos e `caractere`** | Um texto sozinho no `escreva` é ignorado. Em qualquer outro uso (atribuição como `nome <- "Ana"`, argumento, `retorne` ou comparação), o valor faltaria na pilha, então o compilador para com `NAO SUPORTADO: geracao de codigo para textos fora do escreva`. Variáveis `caractere` podem ser declaradas, lidas, copiadas e comparadas, mas `LEIT` e `IMPR` trabalham só com números. |
| **Recursão** | Compila, mas não executa corretamente: como os parâmetros são posições globais, a chamada recursiva sobrescreve os da chamada anterior. |
| **Quebra de linha do `escreval`** | Não representável na MEPA. |
| **Instruções de rotina** | `CHPR`, `ENPR` e `RTPR` não aparecem nos slides; foram usadas no formato do livro de onde a MEPA se origina (seção 8.4). |

Por causa dos vetores, o teste `05_repeticao_vetor.alg` passa nas análises, mas para na geração de código. Nos testes `03_operadores.alg` e `06_procedimento_funcao.alg`, o `mod` foi trocado no fonte pela fórmula `a - (a \ b) * b`, e eles executam por inteiro.

---

## 5. Integração da análise semântica com a geração de código

As duas acontecem na mesma passagem, dentro das funções do parser, sempre na mesma ordem: **primeiro a verificação, depois a emissão**. Assim, nenhuma instrução é gerada para uma construção inválida.

- **Minitabela `TNo`:** variáveis e parâmetros entram na declaração (`decl_var` e `parametro`, via `registrar_var`), e todo `CRVL` ou `ARMZ` de variável é precedido por `busca_tabela_simbolos`.
- **Tabela de rotinas:** guarda o rótulo (usado no `CHPR`), o nome e o tipo de cada parâmetro (usados nos `ARMZ` da chamada e na regra 6) e o tipo de retorno (regra 9).
- **Tipos:** as funções de expressão (`expr` até `fator`) emitem o código e devolvem o tipo, e quem usa o valor confere o tipo antes de emitir a instrução seguinte.
- **Textos:** como um texto não gera código, o contador `textos_sem_codigo` marca as expressões que contêm texto. Ele é consultado **depois** da conferência de tipos, para que `x <- "texto"` com `x` inteiro continue sendo erro de tipo, e não "não suportado".

---

## 6. Testes

### Válidos (geração de código)

| Teste | O que verifica | Saída esperada no `mepa.py` |
|---|---|---|
| `03_operadores` | lógicos, divisão inteira, resto pela fórmula `a - (a \ b) * b`, `<>` | `1 4 2 1` |
| `06_procedimento_funcao` | procedimento sem parâmetros e função lógica com `retorne` dentro de `se` | `14 1` |
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

Os testes léxicos e sintáticos estão no [README](../README.md), seção 2. Os semânticos e de recurso não suportado são estes; em todos, o `saida_mepa.txt` é apagado:

| Teste | Mensagem |
|---|---|
| `semantico_passo_zero` | `passo do para nao pode ser zero` |
| `semantico_passo_variavel` | `passo do para deve ser uma constante inteira` |
| `semantico_retorne_fora_funcao` | `retorne so pode ser usado dentro de uma funcao` |
| `semantico_funcao_como_procedimento` | `funcao nao pode ser usada como procedimento` |
| `semantico_procedimento_em_expressao` | `procedimento nao pode ser usado em expressao` |
| `semantico_numero_argumentos` | `quantidade de argumentos incompativel com a declaracao da rotina` |
| `semantico_nao_declarada` | `variavel nao declarada` |
| `semantico_palavra_reservada` | `palavra reservada nao pode ser usada como nome de variavel` |
| `semantico_redeclaracao` | `variavel declarada mais de uma vez` |
| `semantico_variavel_nome_rotina` | `nome ja usado por uma rotina` |
| `semantico_rotina_nome_variavel` | `nome ja usado por uma variavel` |
| `semantico_rotina_duplicada` | `rotina declarada mais de uma vez` |
| `semantico_nao_usada` | `variavel x recebe valor mas nunca e usada` |
| `semantico_tipos_expressao` | `operador aritmetico exige operandos numericos` |
| `semantico_tipo_atribuicao` | `tipo incompativel na atribuicao` |
| `semantico_condicao_se` | `condicao do se deve ser logica` |
| `semantico_condicao_enquanto` | `condicao do enquanto deve ser logica` |
| `semantico_para_variavel_real` | `variavel de controle do para deve ser inteira` |
| `semantico_para_limite` | `limite do para deve ser inteiro` |
| `semantico_tipo_argumento` | `tipo do argumento 1 incompativel com o parametro` |
| `semantico_tipo_retorno` | `tipo do retorno incompativel com a declaracao da funcao` |
| `semantico_funcao_sem_retorne` | `funcao sem retorne` |
| `naosuportado_texto_atribuicao` | `NAO SUPORTADO: geracao de codigo para textos fora do escreva` |

---

## 7. Execução do código gerado e a ferramenta `ferramentas/mepa.py`

### 7.1 Por que foi preciso uma máquina virtual

O enunciado pede que o TXT gerado seja executado na máquina virtual MEPA, mas nenhuma foi fornecida. Sem uma VM, a única verificação possível seria conferir o `saida_mepa.txt` à mão, o que não é viável para programas com laços aninhados e rotinas.

### 7.2 A VM encontrada: `maquina-virtual-mepa`, de HugoJF

Encontramos no GitHub o repositório **`HugoJF/maquina-virtual-mepa`** (https://github.com/HugoJF/maquina-virtual-mepa), um projeto de terceiros em JavaScript, com versão no navegador em https://hugojf.github.io/maquina-virtual-mepa/mepa.html. Pelo formato das instruções (por exemplo, `CRVL k,n` com nível léxico e deslocamento), trata-se da MEPA do livro de Tomasz Kowaltowski (seção 8.4).

Nos testes, ela mostrou estas limitações:

- não aceita variáveis pelo nome (`CRVL a`), só nível e deslocamento (`CRVL 0,2`);
- grafa a multiplicação como `MULTI` e não tem `DIVF`;
- o `LEIT` empilha um número aleatório entre 0 e 10, em vez de ler do teclado;
- não mostra as pilhas nem permite executar uma instrução por vez.

Para usá-la, seria preciso converter cada `saida_mepa.txt` à mão, e ainda assim não daria para testar leitura nem divisão real.

### 7.3 A ferramenta `ferramentas/mepa.py`

Por isso foi escrito um interpretador MEPA próprio, em Python 3 e sem dependências, usado **apenas para testar** o código gerado (ele **não faz parte do compilador**). Ele:

- lê o formato gerado pelo compilador (variáveis por nome e rótulos `Ln: NADA`) sem conversão, e aceita também o formato da VM de HugoJF;
- executa todas as instruções usadas, inclusive `DIVF` e as de rotina;
- lê os valores do `LEIT` pelo teclado ou pela opção `--entrada`, e mostra a pilha a cada instrução com `--trace`;
- acusa com a linha do arquivo os erros que indicariam código gerado errado: rótulo não definido, pilha vazia, variável fora da área do `AMEM`, `RTPR` fora de uma rotina, divisão por zero e laço infinito.

---

## 8. Uso de Inteligência Artificial

Conforme exigido pelo enunciado, informamos que a ferramenta **Claude (Anthropic)** foi usada no desenvolvimento da parte de geração de código. O uso se deu principalmente como **tutor e revisor**: a maior parte do código foi escrita pelo integrante e depois revisada pela IA.

### 8.1 Como a IA foi usada

- **Estudo do conteúdo:** explicação de análise semântica, código intermediário e MEPA, com exercícios.
- **Planejamento:** leitura crítica do enunciado e do código da Fase 1, divisão em etapas (infraestrutura, moldura, expressões, entrada e saída, controle, rotinas) e identificação de riscos (o formato `TNo`, a ausência de VM, `mod` e vetores).
- **Decisões de projeto:** discussão das alternativas e dos trade-offs de cada decisão da seção 3, sempre com a decisão final tomada pelo integrante.
- **Revisão de código:** cada função escrita foi revisada, com a indicação de erros de compilação, warnings do `-Wall` e erros de lógica.
- **Testes:** sugestão dos programas de teste e dos resultados esperados, e conferência das execuções.

### 8.2 Trechos com maior com participação da IA

- **Ordem de emissão nas expressões:** a necessidade de guardar o operador, o `op_code`, o valor numérico e o índice do identificador **antes** de consumir o token foi apontada pela IA; o código foi escrito pelo integrante e revisado.
- **`cmd_para`:** a decisão de ler o passo direto do token (o passo aparece antes do corpo no fonte, mas o incremento precisa vir depois) e a escolha entre `CMEG` e `CMAG` pelo sinal foram propostas pela IA.
- **`ferramentas/mepa.py`:** e **Rotinas:** escrito em conjunto com IA.

### 8.3 Análise semântica (minitabela de símbolos)

Na parte de análise semântica, a IA foi usada para:

- ajudar a entender o código escrito pelos colegas;
- navegar entre as seções do `compilador.c`;
- escrever a documentação;
- verificar se a implementação corresponde ao que o enunciado pede;
- depurar erros e auxílio para implementação de partes isoladas;
- Criação dos testes.

### 8.4 Instruções de rotina (`CHPR`, `ENPR`, `RTPR`)

**O problema.** O enunciado pede a geração de código para "todos os comandos e rotinas da linguagem", mas os slides da disciplina sobre a MEPA cobrem apenas expressões, atribuição, desvios, leitura e escrita: não há nenhuma instrução para chamar um procedimento ou uma função, nem para voltar dela.

**A origem da MEPA.** A MEPA (Máquina de Execução para Pascal) foi definida no livro *Implementação de Linguagens de Programação*, de Tomasz Kowaltowski (1983), uma referência clássica no ensino de compiladores no Brasil. O livro define, além das instruções que aparecem nos slides, um conjunto para rotinas:

| Instrução | Função |
|---|---|
| `CHPR p` | chama a rotina que começa no rótulo `p`: empilha o endereço de retorno e desvia |
| `ENPR k` | entrada da rotina de nível léxico `k`: salva o registrador de base `D[k]` e o atualiza |
| `RTPR k,n` | retorno: restaura `D[k]`, volta ao endereço guardado pelo `CHPR` e descarta `n` parâmetros da pilha |

**Como as instruções foram obtidas.** A IA identificou essas instruções lendo o código-fonte da VM de HugoJF (seção 7.2), que as implementa no formato do livro, e reconheceu nelas a MEPA de Kowaltowski. **A IA não teve acesso ao livro**: a semântica descrita acima foi tirada da implementação de HugoJF, e não do texto original. O mesmo comportamento foi reproduzido no `mepa.py`, e o compilador passou a emitir essas instruções, preservando os nomes e o formato do livro para manter a compatibilidade com outras implementações da MEPA.

**Como foram usadas aqui.** No livro, os parâmetros ficam na pilha e são acessados por nível léxico e deslocamento. Nesta implementação, os parâmetros ficam em posições de memória com nome (decisão 9 da seção 3), para respeitar a exigência do enunciado de referenciar variáveis pelo nome. Por isso, nenhum parâmetro é colocado na pilha, e o compilador sempre emite `ENPR 1` (todas as rotinas estão no nível 1, abaixo do programa principal) e `RTPR 1,0` (zero parâmetros a descartar).