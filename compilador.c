/*
 * ============================================================
 *  COMPILADORES - PROJETO FASE 2
 *  Compilador da linguagem MiniVisualg: analise lexica,
 *  sintatica e semantica, e geracao de codigo MEPA
 * ============================================================
 *  Integrantes:
 *    Caio Ariel Cardoso Saraiva  - RA: 10439611
 *    Isabela Hissa Pinto         - RA: 10441873
 *    Kaique Barros Paiva         - RA: 10441787
 *
 *  Compilar (comando do enunciado, sem warnings):
 *    gcc -g -Og -Wall compilador.c -o compilador
 *
 *  Executar:
 *    ./compilador testes/validos/15_rotinas.alg
 *
 *  Saidas:
 *    saida_tokens.txt .. tokens e resultado da analise (tambem na tela)
 *    saida_mepa.txt .... codigo MEPA gerado (Fase 2). Em qualquer erro
 *                        o arquivo e apagado, para nunca sobrar um
 *                        codigo incompleto que pareca valido.
 *
 *  O uso de ferramentas de IA no desenvolvimento esta descrito no
 *  README (secao "Uso de Inteligencia Artificial").
 * ------------------------------------------------------------
 *  ORGANIZACAO DO ARQUIVO
 *
 *    1. Definicao dos tokens .... enum TokenNome, OpRelType,
 *                                 struct Token, struct Rotina,
 *                                 tabelas de nomes e de instrucoes
 *                                 e struct TNo (minitabela)
 *    2. Estado global ........... buffer do fonte, posicao, linha,
 *                                 lookahead, tabela de simbolos,
 *                                 tabela de rotinas, arquivos e
 *                                 inicio da minitabela
 *    3. Funcoes auxiliares ...... leitura do arquivo, tabela de
 *                                 simbolos, tabela de rotinas,
 *                                 formatacao da saida e operacoes
 *                                 da minitabela de simbolos (TNo)
 *    4. Tratamento de erros ..... erros lexico, sintatico e semantico
 *                                 e recursos nao suportados
 *    5. Analisador lexico ....... Etapa 2 (obterToken)
 *    6. Geracao de codigo ....... funcoes de emissao MEPA, rotulos e
 *                                 montagem de chamadas
 *    7. Analisador sintatico .... Etapa 3 (nextToken e a gramatica),
 *                                 que tambem dirige a geracao de codigo
 *    8. main .................... liga tudo
 *
 *  FLUXO DE EXECUCAO (Figura 1 do enunciado)
 *
 *    O parser e quem dirige a analise. Ele chama nextToken(), que
 *    chama obterToken() no lexico, que devolve o proximo token e ja
 *    registra a linha correspondente na tela e no arquivo de saida.
 *    Assim a listagem de tokens da Etapa 2 e a analise sintatica da
 *    Etapa 3 acontecem numa unica leitura do fonte.
 *
 *      main -> nextToken -> obterToken -> reconhecerToken
 *                               |
 *                               +-> imprimirToken -> infoToken
 *
 *    Na Fase 2 a geracao de codigo e dirigida pela sintaxe: cada funcao
 *    do parser, ao reconhecer uma construcao, emite as instrucoes MEPA
 *    correspondentes no saida_mepa.txt. Nao ha arvore intermediaria nem
 *    segunda passada sobre o fonte.
 *
 *      cmd_se -> expr -> ... -> fator -> imprimir("CRVL", ...)
 *             -> imprimirDesvio("DSVF", ...) -> comandos -> ...
 *
 *  O programa termina com codigo 0 quando a compilacao vai ate o fim
 *  sem erros, e com codigo 1 em qualquer erro (de abertura de arquivo,
 *  lexico, sintatico, semantico ou de recurso nao suportado).
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ARQUIVO_SAIDA   "saida_tokens.txt"
#define ARQUIVO_MEPA    "saida_mepa.txt"
#define MAX_LEXEMA      256     /* tamanho maximo de um lexema, com o '\0' */
#define MAX_SIMBOLOS    512     /* entradas da tabela de simbolos          */
#define MAX_LINHA_SAIDA 64      /* tamanho da linha formatada de um token  */
#define MAX_ROTINAS     32      /* procedimentos e funcoes do programa    */
#define MAX_PARAMS      8       /* parametros por rotina                  */
#define MAX_IDS_DECL    32      /* nomes numa mesma linha de declaracao   */

/* ============================================================
 *  1. DEFINICAO DOS TOKENS (Figura 2 do enunciado + extras)
 * ============================================================ */

/*
 * Nomes dos tokens usados pelo parser.
 *
 * Os cinco primeiros valores sao os do enunciado e mantem o mesmo
 * significado. Os demais foram acrescentados porque a linguagem tem
 * simbolos, literais e palavras reservadas que o parser precisa
 * distinguir um a um para escolher a producao certa da gramatica.
 *
 * O criterio usado foi: lexemas que aparecem no mesmo ponto da
 * gramatica viram um unico token com atributo (e o caso de TOKEN_OP_REL,
 * que guarda qual operador relacional era no campo op_code); lexemas
 * que levam a producoes diferentes viram tokens diferentes. Por isso
 * nao existe um TOKEN_KEYWORD generico aqui: reconhecer "se" com um
 * unico teste (lookahead.type == TOKEN_SE) e mais direto do que testar
 * o tipo e depois o sub-codigo da palavra reservada.
 *
 * TOKEN_TOTAL nao e um token: e um sentinela que vale a quantidade de
 * valores do enum e serve para validar o tamanho da tabela nomesTokens.
 *
 * Ver readme.txt, secao de decisoes de design.
 */
typedef enum {
      TOKEN_EOF = 0,
      TOKEN_ID,           /* identificadores (variaveis, funcoes)  */
      TOKEN_NUM_INT,      /* numeros inteiros (ex: 42)             */
      TOKEN_NUM_FLOAT,    /* numeros reais (ex: 3.14)              */
      TOKEN_OP_REL,       /* operadores relacionais                */

      /* literais */
      TOKEN_STRING,       /* "texto entre aspas"                   */

      /* operadores e pontuacao */
      TOKEN_ATRIB,        /* <-  */
      TOKEN_SOMA,         /* +   */
      TOKEN_SUB,          /* -   */
      TOKEN_MULT,         /* *   */
      TOKEN_DIV,          /* /   */
      TOKEN_DIV_INT,      /* \   */
      TOKEN_ABRE_PAR,     /* (   */
      TOKEN_FECHA_PAR,    /* )   */
      TOKEN_ABRE_COL,     /* [   */
      TOKEN_FECHA_COL,    /* ]   */
      TOKEN_VIRGULA,      /* ,   */
      TOKEN_DOIS_PONTOS,  /* :   */
      TOKEN_PONTO_PONTO,  /* ..  */

      /* palavras reservadas (um token para cada) */
      TOKEN_ALGORITMO, TOKEN_VAR, TOKEN_INICIO, TOKEN_FIMALGORITMO,
      TOKEN_INTEIRO, TOKEN_REAL, TOKEN_CARACTERE, TOKEN_LOGICO,
      TOKEN_VETOR, TOKEN_DE,
      TOKEN_SE, TOKEN_ENTAO, TOKEN_SENAO, TOKEN_FIMSE,
      TOKEN_PARA, TOKEN_ATE, TOKEN_PASSO, TOKEN_FACA, TOKEN_FIMPARA,
      TOKEN_ENQUANTO, TOKEN_FIMENQUANTO,
      TOKEN_PROCEDIMENTO, TOKEN_FIMPROCEDIMENTO,
      TOKEN_FUNCAO, TOKEN_FIMFUNCAO, TOKEN_RETORNE,
      TOKEN_LEIA, TOKEN_ESCREVA, TOKEN_ESCREVAL,
      TOKEN_VERDADEIRO, TOKEN_FALSO,
      TOKEN_E, TOKEN_OU, TOKEN_MOD,

      TOKEN_TOTAL         /* sentinela: quantidade de tokens       */
} TokenNome;

/*
 * Nome textual de cada token, usado na listagem de saida e nas
 * mensagens de erro.
 *
 * A tabela e indexada pelo proprio enum (inicializadores designados),
 * entao a ordem das linhas aqui nao importa e nenhuma posicao fica
 * "desalinhada" se um token novo for inserido no meio do enum.
 */
static const char *nomesTokens[] = {
      [TOKEN_EOF]             = "EOF",
      [TOKEN_ID]              = "ID",
      [TOKEN_NUM_INT]         = "NUM_INT",
      [TOKEN_NUM_FLOAT]       = "NUM_FLOAT",
      [TOKEN_OP_REL]          = "OP_REL",
      [TOKEN_STRING]          = "STRING",

      [TOKEN_ATRIB]           = "ATRIB",
      [TOKEN_SOMA]            = "SOMA",
      [TOKEN_SUB]             = "SUB",
      [TOKEN_MULT]            = "MULT",
      [TOKEN_DIV]             = "DIV",
      [TOKEN_DIV_INT]         = "DIV_INT",
      [TOKEN_ABRE_PAR]        = "ABRE_PAR",
      [TOKEN_FECHA_PAR]       = "FECHA_PAR",
      [TOKEN_ABRE_COL]        = "ABRE_COL",
      [TOKEN_FECHA_COL]       = "FECHA_COL",
      [TOKEN_VIRGULA]         = "VIRGULA",
      [TOKEN_DOIS_PONTOS]     = "DOIS_PONTOS",
      [TOKEN_PONTO_PONTO]     = "PONTO_PONTO",

      [TOKEN_ALGORITMO]       = "ALGORITMO",
      [TOKEN_VAR]             = "VAR",
      [TOKEN_INICIO]          = "INICIO",
      [TOKEN_FIMALGORITMO]    = "FIMALGORITMO",
      [TOKEN_INTEIRO]         = "INTEIRO",
      [TOKEN_REAL]            = "REAL",
      [TOKEN_CARACTERE]       = "CARACTERE",
      [TOKEN_LOGICO]          = "LOGICO",
      [TOKEN_VETOR]           = "VETOR",
      [TOKEN_DE]              = "DE",
      [TOKEN_SE]              = "SE",
      [TOKEN_ENTAO]           = "ENTAO",
      [TOKEN_SENAO]           = "SENAO",
      [TOKEN_FIMSE]           = "FIMSE",
      [TOKEN_PARA]            = "PARA",
      [TOKEN_ATE]             = "ATE",
      [TOKEN_PASSO]           = "PASSO",
      [TOKEN_FACA]            = "FACA",
      [TOKEN_FIMPARA]         = "FIMPARA",
      [TOKEN_ENQUANTO]        = "ENQUANTO",
      [TOKEN_FIMENQUANTO]     = "FIMENQUANTO",
      [TOKEN_PROCEDIMENTO]    = "PROCEDIMENTO",
      [TOKEN_FIMPROCEDIMENTO] = "FIMPROCEDIMENTO",
      [TOKEN_FUNCAO]          = "FUNCAO",
      [TOKEN_FIMFUNCAO]       = "FIMFUNCAO",
      [TOKEN_RETORNE]         = "RETORNE",
      [TOKEN_LEIA]            = "LEIA",
      [TOKEN_ESCREVA]         = "ESCREVA",
      [TOKEN_ESCREVAL]        = "ESCREVAL",
      [TOKEN_VERDADEIRO]      = "VERDADEIRO",
      [TOKEN_FALSO]           = "FALSO",
      [TOKEN_E]               = "E",
      [TOKEN_OU]              = "OU",
      [TOKEN_MOD]             = "MOD"
};

/* Se este assert quebrar, faltou (ou sobrou) um nome na tabela acima.
 * O erro aparece na compilacao, e nao como um nome errado na saida. */
_Static_assert(sizeof(nomesTokens) / sizeof(nomesTokens[0]) == TOKEN_TOTAL,
               "nomesTokens dessincronizado com o enum TokenNome");

/*
 * Sub-codigos do atributo dos operadores relacionais.
 *
 * Todos os operadores relacionais aparecem no mesmo ponto da gramatica
 * (a producao expr_rel), entao o parser nao precisa distinguir um do
 * outro: o tipo do token e sempre TOKEN_OP_REL e o operador exato fica
 * guardado no campo attribute.op_code, como sugere a Figura 2.
 *
 * OP_NE foi acrescentado porque o operador de diferenca "<>" aparece
 * nos exemplos do anexo. Vale lembrar que no Visualg a igualdade e um
 * "=" so, e nao "==".
 */
typedef enum {
      OP_LT,      /* <  */
      OP_LE,      /* <= */
      OP_EQ,      /* =  (no Visualg igualdade e um "=" so) */
      OP_GT,      /* >  */
      OP_GE,      /* >= */
      OP_NE,      /* <> */
      OP_TOTAL    /* sentinela: quantidade de operadores    */
} OpRelType;

/* Nome textual de cada sub-codigo de operador relacional.
 * Indexada pelo proprio enum, mesma ideia de nomesTokens. */
static const char *nomesOpRel[] = {
      [OP_LT] = "LT",
      [OP_LE] = "LE",
      [OP_EQ] = "EQ",
      [OP_GT] = "GT",
      [OP_GE] = "GE",
      [OP_NE] = "NE"
};

_Static_assert(sizeof(nomesOpRel) / sizeof(nomesOpRel[0]) == OP_TOTAL,
               "nomesOpRel dessincronizado com o enum OpRelType");

/* Instrucao MEPA de comparacao para cada operador relacional, usada
 * pela geracao de codigo em expr_rel. Indexada pelo proprio enum, como
 * nomesOpRel; o assert abaixo acusa na compilacao se faltar alguma. */
static const char *instrucoesOpRel[] = {
      [OP_LT] = "CMME",
      [OP_LE] = "CMEG",
      [OP_EQ] = "CMIG",
      [OP_GT] = "CMMA",
      [OP_GE] = "CMAG",
      [OP_NE] = "CMDG"
};

_Static_assert(sizeof(instrucoesOpRel) / sizeof(instrucoesOpRel[0]) == OP_TOTAL,
               "instrucoesOpRel dessincronizado com o enum OpRelType");

/*
 * Estrutura do token com a union de atributos (Figura 2).
 *
 * O campo 'type' diz qual dos campos da union esta valendo:
 *
 *    TOKEN_ID, TOKEN_STRING ........... table_index
 *    TOKEN_NUM_INT .................... int_value
 *    TOKEN_NUM_FLOAT .................. float_value
 *    TOKEN_OP_REL ..................... op_code
 *    demais tokens .................... nenhum (a union e ignorada)
 *
 * O campo 'line' guarda a linha do fonte em que o lexema comeca e e
 * usado tanto na listagem quanto nas mensagens de erro.
 */
typedef struct {
      TokenNome type;         /* nome do token                     */
      int line;               /* linha do fonte, para erros        */
      union {
            int table_index;    /* indice na tabela de simbolos    */
            int int_value;      /* valor literal convertido        */
            double float_value; /* valor literal convertido        */
            OpRelType op_code;  /* qual operador relacional era    */
      } attribute;
} Token;

/*
 * Uma rotina declarada no programa (procedimento ou funcao).
 *
 * A minitabela de simbolos (TNo) guarda so variaveis, entao as rotinas
 * ficam numa tabela propria. Ela serve as duas partes da fase 2: a
 * analise semantica usa os tipos para conferir chamadas e retornos, e a
 * geracao de codigo usa o rotulo e os nomes dos parametros para montar
 * a chamada (CHPR).
 *
 * Nomes e parametros sao guardados como indices na tabelaSimbolos, o
 * mesmo atributo table_index do token. Os tipos usam o proprio enum de
 * tokens (TOKEN_INTEIRO, TOKEN_REAL, TOKEN_CARACTERE, TOKEN_LOGICO).
 */
typedef struct {
      int nome;                         /* indice do nome na tabelaSimbolos */
      int ehFuncao;                     /* 1 = funcao, 0 = procedimento     */
      int rotulo;                       /* rotulo do inicio do codigo       */
      int numParams;                    /* quantidade de parametros         */
      int params[MAX_PARAMS];           /* indice de cada parametro, em ordem */
      TokenNome tiposParams[MAX_PARAMS];/* tipo de cada parametro           */
      TokenNome tipoRetorno;            /* tipo de retorno (so funcao)      */
} Rotina;

/* Par lexema -> token usado na tabela de palavras reservadas. */
typedef struct {
      const char *lexema;
      TokenNome token;
} PalavraReservada;

/*
 * Tabela de palavras reservadas do MiniVisualg.
 *
 * O lexico primeiro reconhece qualquer sequencia de letras, digitos e
 * '_' como identificador e so depois consulta esta tabela: se o lexema
 * estiver aqui, o token vira a palavra reservada correspondente. A
 * comparacao ignora maiusculas e minusculas, porque o anexo escreve
 * "MOD" e "E" em maiusculo e o resto em minusculo.
 */
const PalavraReservada palavrasReservadas[] = {
      {"algoritmo", TOKEN_ALGORITMO}, {"var", TOKEN_VAR},
      {"inicio", TOKEN_INICIO}, {"fimalgoritmo", TOKEN_FIMALGORITMO},
      {"inteiro", TOKEN_INTEIRO}, {"real", TOKEN_REAL},
      {"caractere", TOKEN_CARACTERE}, {"logico", TOKEN_LOGICO},
      {"vetor", TOKEN_VETOR}, {"de", TOKEN_DE},
      {"se", TOKEN_SE}, {"entao", TOKEN_ENTAO},
      {"senao", TOKEN_SENAO}, {"fimse", TOKEN_FIMSE},
      {"para", TOKEN_PARA}, {"ate", TOKEN_ATE},
      {"passo", TOKEN_PASSO}, {"faca", TOKEN_FACA},
      {"fimpara", TOKEN_FIMPARA}, {"enquanto", TOKEN_ENQUANTO},
      {"fimenquanto", TOKEN_FIMENQUANTO},
      {"procedimento", TOKEN_PROCEDIMENTO},
      {"fimprocedimento", TOKEN_FIMPROCEDIMENTO},
      {"funcao", TOKEN_FUNCAO}, {"fimfuncao", TOKEN_FIMFUNCAO},
      {"retorne", TOKEN_RETORNE}, {"leia", TOKEN_LEIA},
      {"escreva", TOKEN_ESCREVA}, {"escreval", TOKEN_ESCREVAL},
      {"verdadeiro", TOKEN_VERDADEIRO}, {"falso", TOKEN_FALSO},
      {"e", TOKEN_E}, {"ou", TOKEN_OU}, {"mod", TOKEN_MOD}
};

/* ------------------------------------------------------------
 *  Fase 2 - minitabela de simbolos (TNo)
 * ------------------------------------------------------------ */

/*
 * No da minitabela de simbolos, exatamente como pede o enunciado.
 *
 * A minitabela guarda so as VARIAVEIS declaradas (as rotinas ficam na
 * tabelaRotinas) e e a base da analise semantica: diz se um nome foi
 * declarado, qual o seu endereco e qual o seu tipo. Ela nao substitui a
 * tabelaSimbolos do lexico, que guarda o lexema de todo identificador
 * que aparece no fonte, declarado ou nao.
 *
 * A estrutura e uma lista encadeada. Os nos sao inseridos
 * sempre no FIM e nunca removidos, entao a posicao do no na
 * lista e igual ao seu endereco (ver acessaVarNaTabelaSimbolos).
 *
 * Campos:
 *   ID       - nome da variavel (no maximo 15 caracteres + '\0')
 *   endereco - ordem de declaracao: a primeira variavel e 0, a segunda 1...
 *   tipo     - nome curto do tipo: "int", "real", "char" ou "logico".
 *              O vetor tem 7 posicoes, entao "inteiro" (7 letras + '\0')
 *              e "caractere" nao cabem; por isso os nomes curtos (ver
 *              nomeCurtoDoTipo)
 *   prox     - proximo no, ou NULL no ultimo
 */
typedef struct _TNo {
      char ID [16];
      int endereco;
      char tipo [7];
      struct _TNo *prox;
} TNo;

/* ============================================================
 *  2. ESTADO GLOBAL
 * ============================================================
 *  O fonte inteiro e carregado em memoria de uma vez e o lexico
 *  caminha sobre ele com um indice. Isso evita ficar voltando o
 *  ponteiro do arquivo quando e preciso olhar o proximo caractere
 *  (por exemplo para decidir entre "<", "<=", "<>" e "<-").
 * ============================================================ */

static char *buffer = NULL;         /* codigo fonte inteiro em memoria */
static int posicao = 0;             /* posicao atual no buffer         */
static int linhaAtual = 1;          /* linha atual do fonte            */
static FILE *arquivoSaida = NULL;   /* saida_tokens.txt                */
static FILE *arquivoMepa = NULL;    /* saida_mepa.txt                  */

static Token lookahead;             /* token atual visto pelo parser   */

/* Tabela de simbolos simples: guarda os lexemas de identificadores e
 * de cadeias. O atributo table_index do token e o indice aqui dentro. */
static char tabelaSimbolos[MAX_SIMBOLOS][MAX_LEXEMA];
static int totalSimbolos = 0;

/* Quantidade de posicoes de memoria reservadas pelo AMEM: cada variavel
 * simples ocupa 1, um vetor ocupa o seu tamanho, cada parametro de
 * rotina ocupa 1 e cada funcao ocupa mais 1 para o valor de retorno. */
static int totalPosicoes = 0;

/* Tabela de rotinas (procedimentos e funcoes), na ordem de declaracao. */
static Rotina tabelaRotinas[MAX_ROTINAS];
static int totalRotinas = 0;

/* Rotina cujo corpo esta sendo analisado (NULL no programa principal).
 * Usada por cmd_retorne para saber em qual funcao guardar o valor. */
static Rotina *rotinaAtual = NULL;

/* ------------------------------------------------------------
 *  Fase 2 - minitabela de simbolos (TNo)
 * ------------------------------------------------------------ */

/* Primeiro no da minitabela (NULL enquanto nenhuma variavel foi
 * declarada) e endereco que a proxima variavel declarada vai receber.
 * O endereco e um contador proprio, e nao o totalPosicoes: este soma o
 * tamanho dos vetores e as posicoes extras das funcoes, e serve so para
 * o AMEM. */
static TNo *minitabela = NULL;
static int proxEndereco = 0;

/* Regra 4 (variavel com valor atribuido tem de ser usada). As marcas
 * ficam fora da TNo porque o enunciado define a struct com exatamente
 * quatro campos; os vetores sao indexados pelo endereco da variavel.
 * Como cada variavel e um nome distinto da tabelaSimbolos, o endereco
 * nunca passa de MAX_SIMBOLOS. Por serem globais, comecam zerados. */
static int atribuida[MAX_SIMBOLOS];  /* 1 se recebeu valor (<- ou leia) */
static int usada[MAX_SIMBOLOS];      /* 1 se foi lida numa expressao    */
static int linhaDecl[MAX_SIMBOLOS];  /* linha da declaracao, para o erro */

/* Prototipos: sairComErro e erroSemantico sao definidas na secao 4, mas
 * sao usadas antes dela pelas funcoes de tabela da secao 3. */
void sairComErro(void);
void erroSemantico(int linha, const char *mensagem);

/* ============================================================
 *  3. FUNCOES AUXILIARES
 * ============================================================ */

/*
 * carregarArquivo
 * ------------------------------------------------------------
 * Le o arquivo fonte inteiro para o buffer global, terminando o
 * conteudo com '\0' para que o lexico possa detectar o fim sem
 * precisar guardar o tamanho.
 *
 * O arquivo e aberto em modo binario de proposito: em modo texto o
 * MinGW converte "\r\n" em "\n" e o tamanho devolvido por ftell deixa
 * de bater com o que foi lido. Lendo em binario, o '\r' sobra no
 * buffer e e descartado depois por pularEspacosEComentarios.
 *
 * Parametro: nomeArquivo - caminho do fonte .alg
 * Retorno:   1 em caso de sucesso, 0 se nao conseguiu abrir o arquivo
 *            ou alocar memoria.
 */
int carregarArquivo(const char *nomeArquivo)
{
      FILE *arquivo = fopen(nomeArquivo, "rb");
      if (arquivo == NULL)
            return 0;

      fseek(arquivo, 0, SEEK_END);
      long tamanho = ftell(arquivo);
      fseek(arquivo, 0, SEEK_SET);

      buffer = malloc(tamanho + 1);
      if (buffer == NULL)
      {
            fclose(arquivo);
            return 0;
      }

      size_t lidos = fread(buffer, 1, tamanho, arquivo);

      if (lidos != (size_t)tamanho)
      {
            free(buffer);
            buffer = NULL;
            fclose(arquivo);
            return 0;
      }

      buffer[tamanho] = '\0';

      fclose(arquivo);
      return 1;
}

/*
 * inserirSimbolo
 * ------------------------------------------------------------
 * Procura o lexema na tabela de simbolos e, se ainda nao existir,
 * insere no fim. A busca e linear porque a tabela e pequena e o
 * projeto nao exige desempenho nesta fase.
 *
 * Como o mesmo lexema sempre devolve o mesmo indice, duas ocorrencias
 * da variavel "idade" no fonte recebem o mesmo atributo na listagem.
 *
 * Parametro: lexema - texto ja terminado em '\0'
 * Retorno:   indice do lexema na tabela.
 */
int inserirSimbolo(const char *lexema)
{
      for (int i = 0; i < totalSimbolos; i++)
      {
            if (strcmp(tabelaSimbolos[i], lexema) == 0)
                  return i;
      }

      /* Tabela cheia: e melhor parar com uma mensagem clara do que
       * escrever fora do vetor e corromper a memoria. */
      if (totalSimbolos >= MAX_SIMBOLOS)
      {
            printf("ERRO: tabela de simbolos cheia (limite de %d entradas)\n",
                   MAX_SIMBOLOS);
            if (arquivoSaida != NULL)
            {
                  fprintf(arquivoSaida,
                          "ERRO: tabela de simbolos cheia (limite de %d entradas)\n",
                          MAX_SIMBOLOS);
            }
            sairComErro();
      }

      strncpy(tabelaSimbolos[totalSimbolos], lexema, MAX_LEXEMA - 1);
      tabelaSimbolos[totalSimbolos][MAX_LEXEMA - 1] = '\0';
      return totalSimbolos++;
}

/*
 * inserirRotina
 * ------------------------------------------------------------
 * Registra um procedimento ou uma funcao na tabela de rotinas. E chamada
 * na declaracao, antes do corpo, para que o nome ja seja conhecido se o
 * corpo chamar a propria rotina. O rotulo e o tipo de retorno sao
 * preenchidos depois, por quem chamou.
 *
 * Parametros: nome     - indice do nome na tabelaSimbolos
 *             ehFuncao - 1 para funcao, 0 para procedimento
 * Retorno:    ponteiro para a rotina criada, para o parser completar os
 *             campos e acrescentar os parametros.
 */
Rotina *inserirRotina(int nome, int ehFuncao)
{
      if (totalRotinas >= MAX_ROTINAS)
      {
            printf("ERRO: tabela de rotinas cheia (limite de %d rotinas)\n",
                   MAX_ROTINAS);
            fprintf(arquivoSaida,
                    "ERRO: tabela de rotinas cheia (limite de %d rotinas)\n",
                    MAX_ROTINAS);
            sairComErro();
      }

      Rotina *r = &tabelaRotinas[totalRotinas];
      r->nome = nome;
      r->ehFuncao = ehFuncao;
      r->numParams = 0;

      totalRotinas++;

      return r;
}

/*
 * adicionarParametro
 * ------------------------------------------------------------
 * Acrescenta um parametro formal ao fim da lista da rotina, preservando
 * a ordem da declaracao (a chamada depende dessa ordem para casar cada
 * argumento com o seu parametro).
 *
 * Parametros: r    - rotina que recebe o parametro
 *             nome - indice do nome do parametro na tabelaSimbolos
 *             tipo - tipo declarado (TOKEN_INTEIRO, TOKEN_REAL, ...)
 */
void adicionarParametro(Rotina *r, int nome, TokenNome tipo)
{
      if (r->numParams >= MAX_PARAMS)
      {
            printf("ERRO: rotina com parametros demais (limite de %d)\n",
                   MAX_PARAMS);
            fprintf(arquivoSaida,
                    "ERRO: rotina com parametros demais (limite de %d)\n",
                    MAX_PARAMS);
            sairComErro();
      }

      r->params[r->numParams] = nome;
      r->tiposParams[r->numParams] = tipo;

      r->numParams++;
}

/*
 * buscarRotina
 * ------------------------------------------------------------
 * Procura uma rotina pelo indice do nome. Comparar indices basta porque
 * o lexico sempre devolve o mesmo indice para o mesmo lexema.
 *
 * Parametro: nome - indice do nome na tabelaSimbolos
 * Retorno:   ponteiro para a rotina, ou NULL se o identificador nao for
 *            um procedimento nem uma funcao declarados.
 */
Rotina *buscarRotina(int nome)
{
      for (int i = 0; i < totalRotinas; i++)
      {
            if (tabelaRotinas[i].nome == nome)
                  return &tabelaRotinas[i];
      }
      return NULL;
}

/*
 * nomeDoToken
 * ------------------------------------------------------------
 * Traduz um valor do enum TokenNome para o texto que aparece na
 * listagem e nas mensagens de erro.
 *
 * Parametro: tipo - valor do enum
 * Retorno:   nome do token, ou "DESCONHECIDO" se o valor estiver fora
 *            da faixa valida (nao deve acontecer, e so uma protecao).
 */
const char *nomeDoToken(TokenNome tipo)
{
      if ((int)tipo < 0 || tipo >= TOKEN_TOTAL || nomesTokens[tipo] == NULL)
            return "DESCONHECIDO";
      return nomesTokens[tipo];
}

/*
 * nomeDoOpRel
 * ------------------------------------------------------------
 * Mesma ideia de nomeDoToken, mas para o sub-codigo guardado no
 * atributo dos operadores relacionais.
 *
 * Parametro: op - valor do enum OpRelType
 * Retorno:   nome do operador, ou "DESCONHECIDO" fora da faixa.
 */
const char *nomeDoOpRel(OpRelType op)
{
      if ((int)op < 0 || op >= OP_TOTAL || nomesOpRel[op] == NULL)
            return "DESCONHECIDO";
      return nomesOpRel[op];
}

/*
 * instrucaoDoOpRel
 * ------------------------------------------------------------
 * Traduz um operador relacional para a instrucao MEPA de comparacao
 * (OP_LT -> CMME, OP_GE -> CMAG, ...). Mesma protecao de faixa de
 * nomeDoOpRel.
 *
 * Parametro: op - valor do enum OpRelType
 * Retorno:   nome da instrucao, ou "DESCONHECIDO" fora da faixa.
 */
const char *instrucaoDoOpRel(OpRelType op)
{
      if ((int)op < 0 || op >= OP_TOTAL || instrucoesOpRel[op] == NULL)
            return "DESCONHECIDO";
      return instrucoesOpRel[op];
}

/*
 * infoToken
 * ------------------------------------------------------------
 * Monta em 'destino' a linha de saida de um token, no formato pedido
 * pelo enunciado:
 *
 *    linha# NOME | atributo
 *
 * O atributo muda conforme o tipo do token: indice na tabela de
 * simbolos (ID e STRING), valor ja convertido (NUM_INT e NUM_FLOAT),
 * nome do operador (OP_REL) ou '-' quando o token nao tem atributo.
 *
 * O buffer vem do chamador em vez de ser um 'static' interno. Assim
 * duas chamadas dentro do mesmo printf nao sobrescrevem uma a outra,
 * desde que cada uma receba o seu proprio buffer.
 *
 * Parametros: token   - token a formatar
 *             destino - buffer de saida
 *             tamanho - capacidade de 'destino', em bytes
 * Retorno:    o proprio 'destino', para permitir uso direto dentro de
 *             um printf.
 */
char *infoToken(Token token, char *destino, size_t tamanho)
{
      if (destino == NULL || tamanho == 0)
            return destino;

      switch (token.type)
      {
            case TOKEN_ID:
            case TOKEN_STRING:
                  snprintf(destino, tamanho, "%d# %s | %d",
                           token.line, nomeDoToken(token.type),
                           token.attribute.table_index);
                  break;
            case TOKEN_NUM_INT:
                  snprintf(destino, tamanho, "%d# %s | %d",
                           token.line, nomeDoToken(token.type),
                           token.attribute.int_value);
                  break;
            case TOKEN_NUM_FLOAT:
                  snprintf(destino, tamanho, "%d# %s | %g",
                           token.line, nomeDoToken(token.type),
                           token.attribute.float_value);
                  break;
            case TOKEN_OP_REL:
                  snprintf(destino, tamanho, "%d# %s | %s",
                           token.line, nomeDoToken(token.type),
                           nomeDoOpRel(token.attribute.op_code));
                  break;
            default:
                  snprintf(destino, tamanho, "%d# %s | -",
                           token.line, nomeDoToken(token.type));
                  break;
      }

      return destino;
}

/*
 * imprimirToken
 * ------------------------------------------------------------
 * Escreve a linha formatada do token na tela e no arquivo de saida,
 * como pede a Etapa 2 (o mesmo resultado nos dois lugares).
 *
 * Parametro: token - token a registrar
 */
void imprimirToken(Token token)
{
      char linhaSaida[MAX_LINHA_SAIDA];
      infoToken(token, linhaSaida, sizeof(linhaSaida));
      printf("%s\n", linhaSaida);
      fprintf(arquivoSaida, "%s\n", linhaSaida);
}

/*
 * igualSemCaixa
 * ------------------------------------------------------------
 * Compara dois lexemas ignorando maiusculas e minusculas.
 *
 * Foi escrita a mao porque strcasecmp e uma extensao POSIX e o MinGW,
 * que e o compilador usado na correcao, nao garante a funcao.
 *
 * Parametros: a, b - cadeias terminadas em '\0'
 * Retorno:    1 quando sao iguais, 0 caso contrario.
 */
int igualSemCaixa(const char *a, const char *b)
{
      while (*a != '\0' && *b != '\0')
      {
            if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
                  return 0;
            a++;
            b++;
      }
      return *a == *b;
}

/* ------------------------------------------------------------
 *  Fase 2 - minitabela de simbolos (TNo)
 * ------------------------------------------------------------
 *  O acesso a uma variavel e feito em dois passos, simulando o comportamento
 *   de um compilador real: primeiro o NOME e resolvido para um ENDERECO
 *  (busca_tabela_simbolos, que tambem confere se foi declarada) e, dali
 *  em diante, so o endereco circula (acessaVarNaTabelaSimbolos).
 *  (Também fizemos assim para respeitar o enunciado do proj 2.
 *  busca_tabela_simbolos deve retornar Int)
 *
 *    inserirVarSemant ............ declaracao: cria o no no fim da lista
 *    busca_tabela_simbolos ....... uso: nome -> endereco (ou erro)
 *    acessaVarNaTabelaSimbolos ... endereco -> no (tipo, nome...)
 *    liberarTabelaSimbolos ....... libera os nos no fim da execucao
 * ------------------------------------------------------------ */

/*
 * nomeCurtoDoTipo
 * ------------------------------------------------------------
 * Traduz o tipo basico lido pelo parser para o nome guardado no campo
 * tipo do TNo. Os nomes sao curtos porque o campo tem so 7 posicoes
 * (ver comentario da struct TNo).
 *
 * Parametro: tipo - TOKEN_INTEIRO, TOKEN_REAL, TOKEN_CARACTERE ou
 *                   TOKEN_LOGICO
 * Retorno:   "int", "real", "char" ou "logico" ("?" se o valor nao for
 *            um tipo basico; nao deve acontecer, e so uma protecao).
 */
const char *nomeCurtoDoTipo(TokenNome tipo)
{
      switch (tipo)
      {
            case TOKEN_INTEIRO:   return "int";
            case TOKEN_REAL:      return "real";
            case TOKEN_CARACTERE: return "char";
            case TOKEN_LOGICO:    return "logico";
            default:              return "?";
      }
}

/*
 * inserirVarSemant
 * ------------------------------------------------------------
 * Registra uma variavel declarada no FIM da minitabela, com o proximo
 * endereco livre.
 *
 * Como inserir no fim ja exige percorrer a lista inteira, o mesmo laco
 * confere se o nome ja foi declarado (regra 3 do enunciado:
 * identificador unico). Assim a insercao nao precisa de uma funcao de
 * busca separada.
 *
 * A comparacao usa strcmp (diferencia maiusculas e minusculas), o mesmo
 * criterio do inserirSimbolo do lexico: para o compilador, "Idade" e
 * "idade" sao nomes diferentes desde a analise lexica.
 *
 * Parametros: nome  - nome da variavel
 *             tipo  - tipo basico declarado (TOKEN_INTEIRO, ...)
 *             linha - linha da declaracao, para as mensagens de erro
 */
void inserirVarSemant(const char *nome, TokenNome tipo, int linha)
{
      /* o campo ID tem 16 posicoes: truncar o nome poderia fazer dois
       * nomes diferentes virarem iguais, entao e melhor recusar */
      if (strlen(nome) > 15)
            erroSemantico(linha, "nome de variavel com mais de 15 caracteres");

      TNo *ultimo = NULL;
      for (TNo *atual = minitabela; atual != NULL; atual = atual->prox)
      {
            if (strcmp(atual->ID, nome) == 0)
                  erroSemantico(linha, "variavel declarada mais de uma vez");
            ultimo = atual;
      }

      TNo *novo = malloc(sizeof(TNo));
      if (novo == NULL)
      {
            printf("ERRO: memoria insuficiente para a tabela de simbolos\n");
            fprintf(arquivoSaida,
                    "ERRO: memoria insuficiente para a tabela de simbolos\n");
            sairComErro();
      }

      /* os tamanhos ja foram garantidos acima e em nomeCurtoDoTipo, entao
       * strcpy nao passa do fim dos vetores */
      strcpy(novo->ID, nome);
      strcpy(novo->tipo, nomeCurtoDoTipo(tipo));
      novo->endereco = proxEndereco++;
      novo->prox = NULL;

      /* lista vazia: o novo no vira o primeiro */
      if (ultimo == NULL)
            minitabela = novo;
      else
            ultimo->prox = novo;
}

/*
 * busca_tabela_simbolos
 * ------------------------------------------------------------
 * Funcao pedida pelo enunciado: recebe o nome do identificador (o
 * atributo do token corrente) e devolve o endereco da variavel na
 * minitabela. Se o nome nao foi declarado, e erro semantico e a
 * compilacao termina (regra 1: variavel declarada antes do uso).
 *
 * Por isso quem chama nunca precisa testar o retorno: se a funcao
 * voltou, a variavel existe.
 *
 * Nome, parametro atributo_ID e retorno seguem o enunciado. O parametro
 * linha foi acrescentado para que a mensagem de erro informe a linha do
 * fonte, como todas as outras mensagens de erro do compilador.
 *
 * Parametros: atributo_ID - nome da variavel (ver nomeDoSimbolo)
 *             linha       - linha do uso, para a mensagem de erro
 * Retorno:    endereco da variavel.
 */
int busca_tabela_simbolos(const char *atributo_ID, int linha)
{
      for (TNo *atual = minitabela; atual != NULL; atual = atual->prox)
      {
            if (strcmp(atual->ID, atributo_ID) == 0)
                  return atual->endereco;
      }

      erroSemantico(linha, "variavel nao declarada");
      return -1;  /* nunca chega aqui: erroSemantico encerra o programa */
}

/*
 * existeNaMinitabela
 * ------------------------------------------------------------
 * Diz se ha uma variavel com esse nome na minitabela, sem gerar erro.
 * Usada por procedimento e funcao para impedir que uma rotina tenha o
 * nome de uma variavel ja declarada (regra 3). Nao da para usar
 * busca_tabela_simbolos aqui, porque ela da erro quando NAO acha.
 *
 * Parametro: nome - nome a procurar
 * Retorno:   1 se existe, 0 se nao.
 */
int existeNaMinitabela(const char *nome)
{
      for (TNo *atual = minitabela; atual != NULL; atual = atual->prox)
      {
            if (strcmp(atual->ID, nome) == 0)
                  return 1;
      }
      return 0;
}

/*
 * acessaVarNaTabelaSimbolos
 * ------------------------------------------------------------
 * Devolve o no da variavel a partir do endereco obtido em
 * busca_tabela_simbolos, para consultar o tipo (e os demais campos).
 *
 * Nao compara nomes: como os nos sao inseridos sempre no fim e nunca
 * removidos, o no de endereco k e o k-esimo da lista, entao basta
 * andar k passos. Se a insercao mudar (no inicio, por exemplo), esta
 * funcao deixa de valer.
 *
 * O acesso por endereco e "simulado": numa lista encadeada ele continua
 * percorrendo os nos. Num compilador maior, a tabela seria um vetor e o
 * endereco seria o indice direto. Ou seria um hashmap, também com acesso direto.
 *
 * Parametro: endereco - endereco devolvido por busca_tabela_simbolos
 * Retorno:   o no, ou NULL se o endereco nao existir (nao deve acontecer
 *            com um endereco vindo de busca_tabela_simbolos).
 */
TNo *acessaVarNaTabelaSimbolos(int endereco)
{
      TNo *atual = minitabela;
      for (int i = 0; i < endereco && atual != NULL; i++)
            atual = atual->prox;
      return atual;
}

/*
 * liberarTabelaSimbolos
 * ------------------------------------------------------------
 * Libera todos os nos da minitabela. E chamada nas duas saidas do
 * programa: no fim do main e em sairComErro.
 */
void liberarTabelaSimbolos(void)
{
      TNo *atual = minitabela;
      while (atual != NULL)
      {
            /* guarda o proximo antes do free: depois dele o no nao pode
             * mais ser lido */
            TNo *proximo = atual->prox;
            free(atual);
            atual = proximo;
      }
      minitabela = NULL;
}

/* ------------------------------------------------------------
 *  Fase 2 - tipos das expressoes (regra 5)
 * ------------------------------------------------------------
 *  As expressoes devolvem o tipo como TokenNome (TOKEN_INTEIRO,
 *  TOKEN_REAL, TOKEN_CARACTERE ou TOKEN_LOGICO), a mesma representacao
 *  usada nos parametros e no retorno das rotinas. Regra de mistura
 *  combinada pelo grupo (ver README):
 *
 *    + - *  ........ numeros; real se algum lado for real
 *    /  ............ numeros; sempre real (gera DIVF)
 *    \  mod ........ so inteiros; resultado inteiro
 *    < > <= >= ..... numeros (inteiro e real misturam); resultado logico
 *    =  <> ......... numeros, ou dois valores do mesmo tipo; logico
 *    e  ou ......... so logicos
 *    atribuicao .... mesmo tipo, ou inteiro guardado em real
 * ------------------------------------------------------------ */

/*
 * tipoDaVariavel
 * ------------------------------------------------------------
 * Caminho inverso de nomeCurtoDoTipo: le o tipo guardado na minitabela
 * ("int", "real"...) e devolve o TokenNome correspondente, para que as
 * variaveis sejam comparadas com literais, parametros e retornos.
 *
 * Parametro: endereco - endereco devolvido por busca_tabela_simbolos
 * Retorno:   TOKEN_INTEIRO, TOKEN_REAL, TOKEN_CARACTERE ou TOKEN_LOGICO.
 */
TokenNome tipoDaVariavel(int endereco)
{
      const char *tipo = acessaVarNaTabelaSimbolos(endereco)->tipo;

      if (strcmp(tipo, nomeCurtoDoTipo(TOKEN_INTEIRO)) == 0)
            return TOKEN_INTEIRO;
      if (strcmp(tipo, nomeCurtoDoTipo(TOKEN_REAL)) == 0)
            return TOKEN_REAL;
      if (strcmp(tipo, nomeCurtoDoTipo(TOKEN_CARACTERE)) == 0)
            return TOKEN_CARACTERE;
      return TOKEN_LOGICO;
}

/*
 * ehNumerico
 * ------------------------------------------------------------
 * Retorno: 1 se o tipo e inteiro ou real, 0 caso contrario.
 */
int ehNumerico(TokenNome tipo)
{
      return tipo == TOKEN_INTEIRO || tipo == TOKEN_REAL;
}

/*
 * tipoAritmetico
 * ------------------------------------------------------------
 * Confere os dois lados de "+", "-" ou "*" e calcula o tipo do
 * resultado: real se algum lado for real, inteiro se os dois forem.
 *
 * Parametros: a, b  - tipos dos operandos
 *             linha - linha do operador, para a mensagem de erro
 * Retorno:    TOKEN_INTEIRO ou TOKEN_REAL.
 */
TokenNome tipoAritmetico(TokenNome a, TokenNome b, int linha)
{
      if (!ehNumerico(a) || !ehNumerico(b))
            erroSemantico(linha, "operador aritmetico exige operandos numericos");

      if (a == TOKEN_REAL || b == TOKEN_REAL)
            return TOKEN_REAL;
      return TOKEN_INTEIRO;
}

/*
 * tipoAtribuivel
 * ------------------------------------------------------------
 * Diz se um valor do tipo origem pode ser guardado numa posicao do tipo
 * destino: mesmo tipo, ou inteiro guardado em real (nao perde nada).
 * Real em inteiro nao pode, porque perderia a parte decimal.
 *
 * A mesma regra vale para a atribuicao, para os argumentos de uma
 * chamada (regra 6) e para o valor do retorne (regra 9).
 *
 * Parametros: destino - tipo da variavel, parametro ou funcao
 *             origem  - tipo da expressao
 * Retorno:    1 se pode, 0 se nao.
 */
int tipoAtribuivel(TokenNome destino, TokenNome origem)
{
      if (destino == origem)
            return 1;
      return destino == TOKEN_REAL && origem == TOKEN_INTEIRO;
}

/* ============================================================
 *  4. TRATAMENTO DE ERROS
 * ============================================================
 *  Todas as funcoes de erro seguem o que o enunciado pede: mostram a
 *  mensagem com a linha do fonte e encerram todo o processo. O
 *  encerramento passa sempre por sairComErro, que fecha os arquivos
 *  (senao os tokens ja escritos ficariam presos no buffer do stdio e o
 *  arquivo sairia vazio justamente nos testes de erro) e apaga o
 *  saida_mepa.txt incompleto.
 * ============================================================ */

/*
 * sairComErro
 * ------------------------------------------------------------
 * Ponto unico de saida em caso de erro: fecha o saida_tokens.txt,
 * fecha e apaga o saida_mepa.txt (um codigo MEPA pela metade pareceria
 * valido), libera o buffer do fonte e a minitabela de simbolos e
 * termina com codigo 1.
 *
 * Os testes de NULL cobrem erros que acontecem antes de os dois
 * arquivos estarem abertos.
 */
void sairComErro(void)
{
      if (arquivoSaida != NULL)
            fclose(arquivoSaida);

      if (arquivoMepa != NULL)
      {
            fclose(arquivoMepa);
            remove(ARQUIVO_MEPA);
      }

      free(buffer);
      liberarTabelaSimbolos();
      exit(1);
}

/*
 * erroSintatico
 * ------------------------------------------------------------
 * Relata um token que nao era o esperado pela gramatica e encerra o
 * programa com codigo 1.
 *
 * Parametros: token    - token encontrado (traz a linha do fonte)
 *             esperado - texto do que a gramatica esperava ali
 */
void erroSintatico(Token token, const char *esperado)
{
      printf("%d# ERRO SINTATICO: esperado %s, encontrado %s\n",
             token.line, esperado, nomeDoToken(token.type));
      fprintf(arquivoSaida, "%d# ERRO SINTATICO: esperado %s, encontrado %s\n",
              token.line, esperado, nomeDoToken(token.type));
      sairComErro();
}

/*
 * erroLexico
 * ------------------------------------------------------------
 * Relata uma sequencia que nao forma nenhum lexema valido e encerra o
 * programa com codigo 1.
 *
 * Parametros: linha     - linha do fonte onde o erro foi localizado
 *             sequencia - trecho lexicamente errado
 */
void erroLexico(int linha, const char *sequencia)
{
      printf("%d# ERRO LEXICO: '%s'\n", linha, sequencia);
      fprintf(arquivoSaida, "%d# ERRO LEXICO: '%s'\n", linha, sequencia);
      sairComErro();
}

/*
 * erroSemantico
 * ------------------------------------------------------------
 * Relata uma construcao sintaticamente correta mas sem sentido para a
 * linguagem (ex.: procedimento usado em expressao) e encerra o programa
 * com codigo 1. Mesmo formato dos erros lexico e sintatico.
 *
 * Parametros: linha    - linha do fonte onde o erro foi localizado
 *             mensagem - descricao do erro
 */
void erroSemantico(int linha, const char *mensagem)
{
      printf("%d# ERRO SEMANTICO: %s\n", linha, mensagem);
      fprintf(arquivoSaida, "%d# ERRO SEMANTICO: %s\n", linha, mensagem);
      sairComErro();
}

/*
 * naoSuportado
 * ------------------------------------------------------------
 * Encerra a compilacao quando o programa usa um recurso que a geracao
 * de codigo MEPA nao implementa (vetores e o operador mod; ver o
 * README, secao "Limitacoes conhecidas"). O programa do usuario esta correto: a limitacao e do
 * compilador. Por isso a mensagem e diferente do erro semantico, e o
 * saida_mepa.txt incompleto e apagado, como nos outros erros.
 *
 * Parametros: linha   - linha do fonte onde o recurso aparece
 *             recurso - descricao do recurso, como "vetores"
 */
void naoSuportado(int linha, const char *recurso)
{
      printf("%d# NAO SUPORTADO: geracao de codigo para %s\n", linha, recurso);
      fprintf(arquivoSaida, "%d# NAO SUPORTADO: geracao de codigo para %s\n",
              linha, recurso);
      sairComErro();
}

/* ============================================================
 *  5. ANALISADOR LEXICO (SCANNER)  -  Etapa 2
 * ============================================================
 *  Expressoes regulares reconhecidas (ver etapa1_gramatica.txt):
 *
 *    ID        -> (letra | "_") (letra | digito | "_")*
 *    NUM_INT   -> digito+
 *    NUM_FLOAT -> digito+ "." digito+
 *    STRING    -> '"' (qualquer caractere menos '"' e quebra de linha)* '"'
 *    OP_REL    -> "<" | "<=" | "=" | ">" | ">=" | "<>"
 *    comentario-> "//" (qualquer caractere ate o fim da linha)
 *
 *  O enunciado garante que os lexemas vem separados por espaco. Como
 *  alguns exemplos do anexo aparecem sem espaco (escreval("...") por
 *  exemplo), o lexico foi feito para funcionar nos dois casos: ele
 *  sempre decide o fim do lexema pelo proximo caractere, e nao pelo
 *  espaco em branco.
 * ============================================================ */

/*
 * pularEspacosEComentarios
 * ------------------------------------------------------------
 * Avanca a posicao no buffer enquanto encontrar espacos, tabulacoes,
 * quebras de linha, o '\r' dos arquivos do Windows ou comentarios de
 * linha iniciados por "//".
 *
 * E aqui que linhaAtual e incrementada, entao o contador de linhas do
 * programa inteiro depende desta funcao ser chamada antes de comecar
 * a reconhecer cada token.
 */
void pularEspacosEComentarios(void)
{
      while (buffer[posicao] != '\0')
      {
            char c = buffer[posicao];

            if (c == '\n')
            {
                  linhaAtual++;
                  posicao++;
            }
            else if (isspace((unsigned char)c))
            {
                  posicao++;   /* espaco, tab, \r do Windows */
            }
            else if (c == '/' && buffer[posicao + 1] == '/')
            {
                  /* comentario: descarta tudo ate o fim da linha, sem
                   * consumir o '\n' (quem conta a linha e o caso acima) */
                  while (buffer[posicao] != '\n' && buffer[posicao] != '\0')
                        posicao++;
            }
            else
            {
                  break;
            }
      }
}

/*
 * reconhecerToken
 * ------------------------------------------------------------
 * Le o proximo lexema do buffer e devolve o token correspondente,
 * ja com o atributo preenchido. E a funcao que implementa de fato as
 * expressoes regulares da linguagem.
 *
 * A ordem dos testes e: fim de arquivo, numero, cadeia, identificador
 * ou palavra reservada e, por ultimo, os simbolos. Qualquer caractere
 * que nao se encaixe em nenhum desses casos vira erro lexico.
 *
 * E 'static' porque so obterToken deve chama-la: quem passa pelo resto
 * do programa tem que passar por obterToken, que tambem registra o
 * token na listagem.
 *
 * Retorno: o proximo token do fonte (TOKEN_EOF quando o fonte acaba).
 */
static Token reconhecerToken(void)
{
      Token token;

      pularEspacosEComentarios();

      /* a linha e fixada aqui, antes de consumir o lexema, para que um
       * lexema que termine no fim da linha ainda seja reportado na
       * linha em que comecou */
      token.line = linhaAtual;
      token.attribute.int_value = 0;

      /* --- fim do arquivo ------------------------------------- */
      if (buffer[posicao] == '\0')
      {
            token.type = TOKEN_EOF;
            return token;
      }

      /* --- numeros: NUM_INT e NUM_FLOAT ----------------------- */
      if (isdigit((unsigned char)buffer[posicao]))
      {
            char lexema[MAX_LEXEMA];
            int ehReal = 0;
            int inicio = posicao;

            while (isdigit((unsigned char)buffer[posicao]))
                  posicao++;

            if (buffer[posicao] == '.')
            {
                  if (isdigit((unsigned char)buffer[posicao + 1]))
                  {
                        /* 1.60 -> consome o ponto e a parte decimal */
                        ehReal = 1;
                        posicao++;
                        while (isdigit((unsigned char)buffer[posicao]))
                              posicao++;
                  }
                  else if (buffer[posicao + 1] != '.')
                  {
                        /* "1." sem digito depois e um real malformado.
                         * O caso "1..4" (faixa de vetor) cai fora deste
                         * if: o numero termina aqui e o ".." vira o
                         * proximo token. */
                        int parcial = posicao - inicio + 1;
                        if (parcial >= MAX_LEXEMA)
                              parcial = MAX_LEXEMA - 1;
                        memcpy(lexema, buffer + inicio, parcial);
                        lexema[parcial] = '\0';
                        erroLexico(token.line, lexema);
                  }
            }

            int tamanho = posicao - inicio;

            if (tamanho >= MAX_LEXEMA)
            {
                  memcpy(lexema, buffer + inicio, MAX_LEXEMA - 1);
                  lexema[MAX_LEXEMA - 1] = '\0';
                  erroLexico(token.line, lexema);
            }
            memcpy(lexema, buffer + inicio, tamanho);
            lexema[tamanho] = '\0';

            if (ehReal)
            {
                  token.type = TOKEN_NUM_FLOAT;
                  token.attribute.float_value = strtod(lexema, NULL);
            }
            else
            {
                  token.type = TOKEN_NUM_INT;
                  token.attribute.int_value = (int)strtol(lexema, NULL, 10);
            }

            return token;
      }

      /* --- cadeias de caracteres ------------------------------ */
      if (buffer[posicao] == '"')
      {
            char lexema[MAX_LEXEMA];
            int inicio = posicao;

            posicao++;   /* passa da aspa de abertura */

            /* a quebra de linha encerra a busca: uma cadeia nao pode
             * atravessar linhas, entao "abc sem fechar vira erro na
             * propria linha em que comecou */
            while (buffer[posicao] != '"'
                   && buffer[posicao] != '\n'
                   && buffer[posicao] != '\0')
                  posicao++;

            if (buffer[posicao] != '"')
            {
                  /* cadeia aberta: mostra o trecho lido ate onde deu */
                  int parcial = posicao - inicio;
                  if (parcial >= MAX_LEXEMA)
                        parcial = MAX_LEXEMA - 1;
                  memcpy(lexema, buffer + inicio, parcial);
                  lexema[parcial] = '\0';
                  erroLexico(token.line, lexema);
            }

            posicao++;   /* consome a aspa de fechamento */

            int tamanho = posicao - inicio;

            if (tamanho >= MAX_LEXEMA)
            {
                  memcpy(lexema, buffer + inicio, MAX_LEXEMA - 1);
                  lexema[MAX_LEXEMA - 1] = '\0';
                  erroLexico(token.line, lexema);
            }
            memcpy(lexema, buffer + inicio, tamanho);
            lexema[tamanho] = '\0';

            /* o lexema e guardado com as aspas, para que a cadeia
             * "idade" e a variavel idade nao ocupem o mesmo indice da
             * tabela de simbolos */
            token.type = TOKEN_STRING;
            token.attribute.table_index = inserirSimbolo(lexema);

            return token;
      }

      /* --- identificadores e palavras reservadas -------------- */
      if (isalpha((unsigned char)buffer[posicao]) || buffer[posicao] == '_')
      {
            char lexema[MAX_LEXEMA];
            int inicio = posicao;

            while (isalnum((unsigned char)buffer[posicao]) || buffer[posicao] == '_')
                  posicao++;

            int tamanho = posicao - inicio;

            /* um lexema maior que o limite vira erro em vez de ser
             * truncado: truncar faria dois identificadores longos e
             * diferentes virarem o mesmo simbolo */
            if (tamanho >= MAX_LEXEMA)
            {
                  memcpy(lexema, buffer + inicio, MAX_LEXEMA - 1);
                  lexema[MAX_LEXEMA - 1] = '\0';
                  erroLexico(token.line, lexema);
            }
            memcpy(lexema, buffer + inicio, tamanho);
            lexema[tamanho] = '\0';

            /* assume identificador e rebaixa para palavra reservada se
             * o lexema estiver na tabela */
            token.type = TOKEN_ID;
            int n = sizeof(palavrasReservadas) / sizeof(palavrasReservadas[0]);
            for (int i = 0; i < n; i++)
            {
                  if (igualSemCaixa(lexema, palavrasReservadas[i].lexema))
                  {
                        token.type = palavrasReservadas[i].token;
                        break;
                  }
            }

            /* so identificador ocupa lugar na tabela de simbolos */
            if (token.type == TOKEN_ID)
                  token.attribute.table_index = inserirSimbolo(lexema);

            return token;
      }

      /* --- operadores e pontuacao ----------------------------- */
      switch (buffer[posicao])
      {
            case '+':
                  token.type = TOKEN_SOMA;
                  posicao++;
                  break;
            case '-':
                  token.type = TOKEN_SUB;
                  posicao++;
                  break;
            case '*':
                  token.type = TOKEN_MULT;
                  posicao++;
                  break;
            case '/':
                  /* o caso "//" ja foi consumido como comentario antes
                   * de chegar aqui, entao esta barra e sempre divisao */
                  token.type = TOKEN_DIV;
                  posicao++;
                  break;
            case '\\':
                  token.type = TOKEN_DIV_INT;
                  posicao++;
                  break;
            case '(':
                  token.type = TOKEN_ABRE_PAR;
                  posicao++;
                  break;
            case ')':
                  token.type = TOKEN_FECHA_PAR;
                  posicao++;
                  break;
            case '[':
                  token.type = TOKEN_ABRE_COL;
                  posicao++;
                  break;
            case ']':
                  token.type = TOKEN_FECHA_COL;
                  posicao++;
                  break;
            case ',':
                  token.type = TOKEN_VIRGULA;
                  posicao++;
                  break;
            case ':':
                  token.type = TOKEN_DOIS_PONTOS;
                  posicao++;
                  break;
            case '=':
                  /* no Visualg a igualdade e um "=" so */
                  token.type = TOKEN_OP_REL;
                  token.attribute.op_code = OP_EQ;
                  posicao++;
                  break;
            case '>':
                  /* ">" ou ">=": e preciso olhar o proximo caractere */
                  posicao++;                          /* consome o '>' */
                  token.type = TOKEN_OP_REL;
                  if (buffer[posicao] == '=')
                  {
                        token.attribute.op_code = OP_GE;
                        posicao++;                    /* consome o '=' */
                  }
                  else
                  {
                        token.attribute.op_code = OP_GT;
                  }
                  break;
            case '<':
                  /* o '<' e o caractere mais ambiguo da linguagem:
                   * pode abrir "<=", "<>" (relacionais) ou "<-"
                   * (atribuicao), e sozinho e o "menor que" */
                  posicao++;                          /* consome o '<' */
                  if (buffer[posicao] == '=')
                  {
                        token.type = TOKEN_OP_REL;
                        token.attribute.op_code = OP_LE;
                        posicao++;
                  }
                  else if (buffer[posicao] == '>')
                  {
                        token.type = TOKEN_OP_REL;
                        token.attribute.op_code = OP_NE;
                        posicao++;
                  }
                  else if (buffer[posicao] == '-')
                  {
                        token.type = TOKEN_ATRIB;
                        posicao++;
                  }
                  else
                  {
                        token.type = TOKEN_OP_REL;
                        token.attribute.op_code = OP_LT;
                  }
                  break;
            case '.':
                  /* so existe ".." na linguagem (faixa de vetor); um
                   * ponto sozinho nao forma lexema nenhum */
                  posicao++;                    /* consome o 1o ponto */
                  if (buffer[posicao] == '.')
                  {
                        token.type = TOKEN_PONTO_PONTO;
                        posicao++;              /* consome o 2o ponto */
                  }
                  else
                  {
                        char seq[2] = { '.', '\0' };
                        erroLexico(token.line, seq);
                  }
                  break;
            default:
            {
                  /* caractere que nao pertence ao alfabeto da linguagem */
                  char seq[2] = { buffer[posicao], '\0' };
                  erroLexico(token.line, seq);
            }
      }

      return token;
}

/*
 * obterToken
 * ------------------------------------------------------------
 * Interface do lexico para o parser (Figura 1 do enunciado).
 *
 * Pede o proximo token ao reconhecedor e registra a linha de saida
 * antes de devolve-lo. Por isso a listagem de tokens da Etapa 2 e
 * produzida numa unica passagem, junto com a analise sintatica da
 * Etapa 3, sem precisar ler o fonte duas vezes.
 *
 * Isso so e seguro porque a gramatica e LL(1): o parser nunca volta
 * atras, entao nenhum token e lido (nem impresso) duas vezes.
 *
 * Retorno: o proximo token do fonte.
 */
Token obterToken(void)
{
      Token token = reconhecerToken();
      imprimirToken(token);
      return token;
}

/* ============================================================
 *  6. GERACAO DE CODIGO MEPA  -  Fase 2
 * ============================================================
 *  A geracao e dirigida pela sintaxe: as funcoes do parser (secao 7)
 *  chamam as funcoes desta secao no momento em que reconhecem cada
 *  construcao, e o codigo sai direto no saida_mepa.txt.
 *
 *  As funcoes de emissao sabem COMO escrever uma linha; o parser decide
 *  QUANDO e O QUE escrever. Ha uma funcao por formato de operando:
 *
 *    imprimir .............. sem operando ......... SOMA, INPP, IMPR
 *    imprimirNome .......... nome de variavel ..... CRVL idade
 *    imprimirInteiro ....... inteiro .............. CRCT 4, AMEM 3
 *    imprimirDoisInteiros .. dois inteiros ........ RTPR 1,0
 *    imprimirReal .......... real ................. CRCT 1.6
 *    imprimirDesvio ........ rotulo usado ......... DSVF L3, CHPR L2
 *    imprimirRotulo ........ rotulo definido ...... L3: NADA
 *
 *  As instrucoes ficam recuadas com 4 espacos e os rotulos na coluna 0,
 *  uma instrucao por linha. As variaveis sao referenciadas pelo nome,
 *  como pede o enunciado.
 * ============================================================ */

/*
 * imprimir
 * ------------------------------------------------------------
 * Escreve uma instrucao sem operando.  Exemplo: "    SOMA"
 *
 * Parametro: instr - mnemonico da instrucao
 */
void imprimir(const char *instr)
{
      fprintf(arquivoMepa, "    %s\n", instr);
}

/*
 * imprimirNome
 * ------------------------------------------------------------
 * Escreve uma instrucao cujo operando e o nome de uma variavel.
 * Exemplo: "    CRVL idade"
 *
 * Parametros: instr - mnemonico da instrucao
 *             nome  - nome da variavel (ver nomeDoSimbolo)
 */
void imprimirNome(const char *instr, const char *nome)
{
      fprintf(arquivoMepa, "    %s %s\n", instr, nome);
}

/*
 * imprimirInteiro
 * ------------------------------------------------------------
 * Escreve uma instrucao com operando inteiro.  Exemplo: "    CRCT 4"
 *
 * Parametros: instr - mnemonico da instrucao
 *             valor - operando
 */
void imprimirInteiro(const char *instr, int valor)
{
      fprintf(arquivoMepa, "    %s %d\n", instr, valor);
}

/*
 * imprimirDoisInteiros
 * ------------------------------------------------------------
 * Escreve uma instrucao com dois operandos inteiros separados por
 * virgula, formato do RTPR k,n.  Exemplo: "    RTPR 1,0"
 *
 * Parametros: instr  - mnemonico da instrucao
 *             valor1 - primeiro operando
 *             valor2 - segundo operando
 */
void imprimirDoisInteiros(const char *instr, int valor1, int valor2)
{
      fprintf(arquivoMepa, "    %s %d,%d\n", instr, valor1, valor2);
}

/*
 * imprimirReal
 * ------------------------------------------------------------
 * Escreve uma instrucao com operando real. O %g evita zeros inuteis
 * (1.6 em vez de 1.600000).  Exemplo: "    CRCT 1.6"
 *
 * Parametros: instr - mnemonico da instrucao
 *             valor - operando
 */
void imprimirReal(const char *instr, double valor)
{
      fprintf(arquivoMepa, "    %s %g\n", instr, valor);
}

/*
 * imprimirDesvio
 * ------------------------------------------------------------
 * Escreve uma instrucao que USA um rotulo (desvios e chamada de
 * rotina).  Exemplo: "    DSVF L3"
 *
 * Parametros: instr  - DSVS, DSVF ou CHPR
 *             rotulo - numero do rotulo (o "L" e acrescentado aqui)
 */
void imprimirDesvio(const char *instr, int rotulo)
{
      fprintf(arquivoMepa, "    %s L%d\n", instr, rotulo);
}

/*
 * imprimirRotulo
 * ------------------------------------------------------------
 * DEFINE um rotulo, marcando a posicao para onde os desvios pulam.
 * O NADA garante que o rotulo sempre tem uma instrucao, mesmo no fim de
 * um bloco.  Exemplo: "L3: NADA"
 *
 * Parametro: rotulo - numero do rotulo
 */
void imprimirRotulo(int rotulo)
{
      fprintf(arquivoMepa, "L%d: NADA\n", rotulo);
}

/*
 * proximo_rotulo
 * ------------------------------------------------------------
 * Funcao sugerida pelo enunciado: devolve o proximo rotulo consecutivo
 * positivo (1, 2, 3, ...). O contador e 'static' local, entao so esta
 * funcao consegue altera-lo.
 *
 * Por causa da recursao do parser, cada comando pede os seus rotulos
 * logo no inicio e guarda em variaveis LOCAIS: assim um comando interno
 * (um se dentro de outro, por exemplo) pede os proprios rotulos sem
 * sobrescrever os do comando externo.
 *
 * Retorno: numero do novo rotulo.
 */
int proximo_rotulo(void)
{
      static int contadorRotulos = 0;
      return ++contadorRotulos;
}

/*
 * nomeDoSimbolo
 * ------------------------------------------------------------
 * Devolve o lexema de um identificador a partir do atributo
 * table_index do token, para ser usado como operando no MEPA.
 *
 * Parametro: indice - indice na tabelaSimbolos
 * Retorno:   o nome (const: quem recebe nao pode alterar a tabela).
 */
const char *nomeDoSimbolo(int indice)
{
      return tabelaSimbolos[indice];
}

/*
 * gerarChamada
 * ------------------------------------------------------------
 * Gera a parte comum a toda chamada de rotina, depois que o codigo dos
 * argumentos ja foi emitido (eles estao empilhados, o ultimo no topo):
 *
 *    ARMZ ultimo_parametro      <- o ARMZ retira o topo, por isso
 *    ...                           os parametros sao percorridos do
 *    ARMZ primeiro_parametro       ultimo para o primeiro
 *    CHPR Lrotina
 *
 * Os parametros sao posicoes de memoria com nome (ver README, decisao
 * sobre rotinas). Para uma funcao, quem chama emite depois o CRVL com
 * o nome dela para buscar o resultado.
 *
 * A quantidade de argumentos e conferida aqui (parte "numero" da regra
 * 6 do enunciado) porque, com a quantidade errada, os ARMZ sairiam
 * descasados.
 *
 * Parametros: r       - rotina chamada
 *             numArgs - quantidade de argumentos da chamada
 *             linha   - linha da chamada, para a mensagem de erro
 */
void gerarChamada(Rotina *r, int numArgs, int linha)
{
      if (numArgs != r->numParams)
            erroSemantico(linha, "quantidade de argumentos incompativel com a declaracao da rotina");

      for (int i = r->numParams - 1; i >= 0; i--)
            imprimirNome("ARMZ", nomeDoSimbolo(r->params[i]));

      imprimirDesvio("CHPR", r->rotulo);
}

/* ============================================================
 *  7. ANALISADOR SINTATICO (PARSER)  -  Etapa 3
 * ============================================================
 *  Analise descendente recursiva: cada nao-terminal da gramatica vira
 *  uma funcao, e a escolha da producao e feita olhando um unico token
 *  a frente (o lookahead). Na Fase 2 as mesmas funcoes fazem as
 *  verificacoes semanticas e emitem o codigo MEPA (secao 6).
 *
 *  Um cuidado aparece em quase todas as funcoes: o atributo de um token
 *  (indice do nome, valor numerico, operador) e guardado numa variavel
 *  local ANTES de consumir o token, porque depois do consumo o
 *  lookahead ja e o token seguinte. A gramatica implementada e:
 *
 *    programa     -> ALGORITMO STRING declaracoes INICIO comandos
 *                    FIMALGORITMO
 *    declaracoes  -> { secao_var | procedimento | funcao }
 *    secao_var    -> VAR { decl_var }
 *    decl_var     -> lista_ids ":" tipo
 *    lista_ids    -> ID { "," ID }
 *    tipo         -> tipo_basico
 *                  | VETOR "[" NUM_INT ".." NUM_INT "]" DE tipo_basico
 *    tipo_basico  -> INTEIRO | REAL | CARACTERE | LOGICO
 *    procedimento -> PROCEDIMENTO ID [ "(" parametros ")" ]
 *                    INICIO comandos FIMPROCEDIMENTO
 *    funcao       -> FUNCAO ID "(" parametros ")" ":" tipo_basico
 *                    INICIO comandos FIMFUNCAO
 *    parametros   -> parametro { "," parametro }
 *    parametro    -> ID ":" tipo_basico
 *    comandos     -> { comando }
 *    comando      -> cmd_id | cmd_se | cmd_para | cmd_enquanto
 *                  | cmd_leia | cmd_escreva | cmd_retorne
 *    cmd_id       -> ID [ "<-" expr
 *                       | "[" expr "]" "<-" expr
 *                       | "(" argumentos ")" ]
 *    cmd_se       -> SE expr ENTAO comandos [ SENAO comandos ] FIMSE
 *    cmd_para     -> PARA ID DE expr ATE expr [ PASSO expr ] FACA
 *                    comandos FIMPARA
 *    cmd_enquanto -> ENQUANTO expr FACA comandos FIMENQUANTO
 *    cmd_leia     -> LEIA "(" variavel ")"
 *    cmd_escreva  -> ( ESCREVA | ESCREVAL ) "(" argumentos ")"
 *    cmd_retorne  -> RETORNE expr
 *    variavel     -> ID [ "[" expr "]" ]
 *    argumentos   -> expr { "," expr }
 *    expr         -> expr_e { OU expr_e }
 *    expr_e       -> expr_rel { E expr_rel }
 *    expr_rel     -> expr_arit [ OP_REL expr_arit ]
 *    expr_arit    -> termo { ( "+" | "-" ) termo }
 *    termo        -> fator { ( "*" | "/" | "\" | MOD ) fator }
 *    fator        -> NUM_INT | NUM_FLOAT | STRING | VERDADEIRO | FALSO
 *                  | "-" fator | "(" expr ")"
 *                  | ID [ "[" expr "]" | "(" argumentos ")" ]
 *
 *  A cadeia expr -> expr_e -> expr_rel -> expr_arit -> termo -> fator
 *  e o que da a precedencia dos operadores: OU e o mais fraco e os
 *  fatores sao os mais fortes. A mesma recursao produz o codigo das
 *  expressoes em ordem pos-fixa: o operador so e emitido depois do
 *  codigo dos dois operandos.
 * ============================================================ */

/*
 * nextToken
 * ------------------------------------------------------------
 * Pede o proximo token ao lexico e guarda em lookahead. E a unica
 * forma de o parser avancar no fonte.
 */
void nextToken(void)
{
      lookahead = obterToken();
}

/*
 * consumir
 * ------------------------------------------------------------
 * Confere se o token atual e o esperado pela producao e avanca. Se
 * nao for, a analise para com erro sintatico.
 *
 * Parametro: esperado - token exigido pela gramatica neste ponto
 */
void consumir(TokenNome esperado)
{
      if (lookahead.type == esperado)
            nextToken();
      else
            erroSintatico(lookahead, nomeDoToken(esperado));
}

/* Prototipos: uma funcao por nao-terminal da gramatica. Sao declarados
 * aqui em cima porque as producoes se chamam em ciclo (expr chega em
 * fator, que volta em expr). */
void conferirVariaveisNaoUsadas(void);
void programa(void);
void declaracoes(void);
void secao_var(void);
void decl_var(void);
int lista_ids(int indices[], int linhas[]);
int tipo(TokenNome *tipoBasico);
TokenNome tipo_basico(void);
void procedimento(void);
void funcao(void);
void parametros(Rotina *r);
void parametro(Rotina *r);
void comandos(void);
void comando(void);
void cmd_id(void);
void cmd_se(void);
void cmd_para(void);
void cmd_enquanto(void);
void cmd_leia(void);
void cmd_escreva(void);
void cmd_retorne(void);
int variavel(void);
int argumentos(void);
TokenNome expr(void);
TokenNome expr_e(void);
TokenNome expr_rel(void);
TokenNome expr_arit(void);
TokenNome termo(void);
TokenNome fator(void);

/*
 * argumentos -> expr { "," expr }
 * ------------------------------------------------------------
 * Lista de expressoes separadas por virgula, usada nas chamadas de
 * procedimentos e funcoes. O codigo de cada argumento e emitido em
 * ordem, deixando os valores empilhados para gerarChamada.
 *
 * O escreva nao usa esta funcao: ele percorre os proprios argumentos
 * para emitir um IMPR depois de cada um (ver cmd_escreva).
 *
 * Retorno: quantidade de argumentos, conferida com a declaracao da
 *          rotina em gerarChamada.
 */
int argumentos(void)
{
      int contador = 1;
      expr();
      while (lookahead.type == TOKEN_VIRGULA)
      {
            consumir(TOKEN_VIRGULA);
            expr();
            contador++;
      }
      return contador;
}

/*
 * expr -> expr_e { OU expr_e }
 * ------------------------------------------------------------
 * Nivel mais externo da expressao: o operador logico OU, que tem a
 * menor precedencia da linguagem.
 *
 * Codigo gerado: <expr_e> <expr_e> DISJ   (1 = verdadeiro, 0 = falso)
 *
 * Verificacao semantica: regra 5 (os dois lados do OU sao logicos).
 *
 * Retorno: tipo da expressao (o de expr_e se nao houver OU; logico se
 *          houver).
 */
TokenNome expr(void)
{
      TokenNome tipo = expr_e();
      while (lookahead.type == TOKEN_OU)
      {
            int linha = lookahead.line;
            consumir(TOKEN_OU);
            TokenNome direita = expr_e();
            if (tipo != TOKEN_LOGICO || direita != TOKEN_LOGICO)
                  erroSemantico(linha, "operador logico exige operandos logicos");
            imprimir("DISJ");
      }
      return tipo;
}

/*
 * expr_e -> expr_rel { E expr_rel }
 * ------------------------------------------------------------
 * Operador logico E, que liga mais forte que o OU.
 *
 * Codigo gerado: <expr_rel> <expr_rel> CONJ
 *
 * Verificacao semantica: regra 5 (os dois lados do E sao logicos).
 *
 * Retorno: tipo da expressao (o de expr_rel se nao houver E; logico se
 *          houver).
 */
TokenNome expr_e(void)
{
      TokenNome tipo = expr_rel();
      while (lookahead.type == TOKEN_E)
      {
            int linha = lookahead.line;
            consumir(TOKEN_E);
            TokenNome direita = expr_rel();
            if (tipo != TOKEN_LOGICO || direita != TOKEN_LOGICO)
                  erroSemantico(linha, "operador logico exige operandos logicos");
            imprimir("CONJ");
      }
      return tipo;
}

/*
 * expr_rel -> expr_arit [ OP_REL expr_arit ]
 * ------------------------------------------------------------
 * Comparacao. E um 'if' e nao um 'while' porque a comparacao nao
 * encadeia: "a < b < c" nao faz sentido na linguagem.
 *
 * Qualquer operador relacional serve aqui, entao basta testar o tipo
 * TOKEN_OP_REL; qual deles era fica no atributo do token.
 *
 * Codigo gerado: <expr_arit> <expr_arit> CMxx, com CMxx escolhida pela
 * tabela instrucoesOpRel. A comparacao deixa 1 ou 0 na pilha.
 *
 * Verificacao semantica: regra 5. < > <= >= comparam so numeros
 * (inteiro e real misturam); = e <> aceitam tambem dois valores do
 * mesmo tipo (caractere com caractere, logico com logico).
 *
 * Retorno: tipo da expressao (o de expr_arit se nao houver comparacao;
 *          logico se houver).
 */
TokenNome expr_rel(void)
{
      TokenNome tipo = expr_arit();
      if (lookahead.type == TOKEN_OP_REL)
      {
            /* operador e linha guardados antes de consumir o token */
            OpRelType op = lookahead.attribute.op_code;
            int linha = lookahead.line;
            consumir(TOKEN_OP_REL);
            TokenNome direita = expr_arit();

            int numeros = ehNumerico(tipo) && ehNumerico(direita);
            int igualdade = (op == OP_EQ || op == OP_NE);
            if (!numeros && !(igualdade && tipo == direita))
                  erroSemantico(linha, "comparacao entre tipos incompativeis");

            imprimir(instrucaoDoOpRel(op));
            tipo = TOKEN_LOGICO;
      }
      return tipo;
}

/*
 * expr_arit -> termo { ( "+" | "-" ) termo }
 * ------------------------------------------------------------
 * Soma e subtracao. Como os dois operadores levam ao mesmo lugar, o
 * avanco e feito direto com nextToken em vez de consumir.
 *
 * Codigo gerado: <termo> <termo> SOMA | SUBT. O laco emite cada operacao
 * assim que o segundo operando termina, o que da a associatividade a
 * esquerda: 10 - 4 - 3 vira (10 - 4) - 3.
 *
 * O "default" do switch evita o warning do -Wall para os valores do
 * enum que nunca chegam aqui.
 *
 * Verificacao semantica: regra 5 (os dois lados sao numeros), em
 * tipoAritmetico.
 *
 * Retorno: tipo da expressao (real se algum operando for real).
 */
TokenNome expr_arit(void)
{
      TokenNome tipo = termo();
      while (lookahead.type == TOKEN_SOMA || lookahead.type == TOKEN_SUB)
      {
            /* guarda operador e linha: depois do nextToken o lookahead ja
             * e o operando da direita */
            TokenNome op = lookahead.type;
            int linha = lookahead.line;
            nextToken();
            TokenNome direita = termo();
            tipo = tipoAritmetico(tipo, direita, linha);

            /* os dois operandos ja estao na pilha: agora vem a operacao */
            switch (op)
            {
                  case TOKEN_SOMA:
                        imprimir("SOMA");
                        break;
                  case TOKEN_SUB:
                        imprimir("SUBT");
                        break;
                  default:
                        break;
            }
      }
      return tipo;
}

/*
 * termo -> fator { ( "*" | "/" | "\" | MOD ) fator }
 * ------------------------------------------------------------
 * Multiplicacao, divisao real, divisao inteira e resto: todos ligam
 * mais forte que soma e subtracao.
 *
 * Codigo gerado: <fator> <fator> MULT | DIVF | DIVI. O operador mod
 * nao e suportado na geracao de codigo e encerra a compilacao.
 *
 * Verificacao semantica: regra 5. "*" e "/" exigem numeros; "\" e mod
 * exigem inteiros. Os tipos sao conferidos antes de emitir o codigo.
 *
 * Retorno: tipo da expressao. "*": real se algum lado for real; "/":
 *          sempre real; "\" e mod: inteiro.
 */
TokenNome termo(void)
{
      TokenNome tipo = fator();
      while (lookahead.type == TOKEN_MULT || lookahead.type == TOKEN_DIV
             || lookahead.type == TOKEN_DIV_INT || lookahead.type == TOKEN_MOD)
      {
            /* operador e linha guardados antes de avancar */
            TokenNome op = lookahead.type;
            int linha = lookahead.line;
            nextToken();
            TokenNome direita = fator();

            switch (op)
            {
                  case TOKEN_MULT:
                        tipo = tipoAritmetico(tipo, direita, linha);
                        imprimir("MULT");
                        break;
                  case TOKEN_DIV:
                        /* no Visualg "/" e sempre divisao real */
                        tipoAritmetico(tipo, direita, linha);
                        tipo = TOKEN_REAL;
                        imprimir("DIVF");
                        break;
                  case TOKEN_DIV_INT:
                  case TOKEN_MOD:
                        if (tipo != TOKEN_INTEIRO || direita != TOKEN_INTEIRO)
                              erroSemantico(linha, "operadores \\ e mod exigem operandos inteiros");
                        tipo = TOKEN_INTEIRO;

                        if (op == TOKEN_DIV_INT)
                              imprimir("DIVI");
                        else
                              /* a MEPA nao tem instrucao de resto (ver README) */
                              naoSuportado(linha, "o operador mod");
                        break;
                  default:
                        break;
            }
      }
      return tipo;
}

/*
 * fator -> NUM_INT | NUM_FLOAT | STRING | VERDADEIRO | FALSO
 *        | "-" fator | "(" expr ")"
 *        | ID [ "[" expr "]" | "(" argumentos ")" ]
 * ------------------------------------------------------------
 * Unidade indivisivel da expressao. Depois de um ID e preciso olhar o
 * proximo token para saber se e uma variavel simples, um elemento de
 * vetor ou uma chamada de funcao.
 *
 * O caso "-" fator e recursivo e cobre o menos unario, como em
 * "passo -2".
 *
 * Codigo gerado:
 *    numero ............ CRCT valor
 *    verdadeiro/falso .. CRCT 1 / CRCT 0
 *    variavel .......... CRVL nome
 *    "-" fator ......... <fator> INVR
 *    funcao(args) ...... <args> ARMZ params CHPR Lf CRVL funcao
 *    "texto" ........... nada (tratado pelo escreva)
 *
 * Verificacoes semanticas: regra 1 (variavel simples declarada antes do
 * uso, via busca_tabela_simbolos), regra 5 (menos unario so em numero),
 * regra 7 (procedimento nao pode ser usado em expressao) e quantidade
 * de argumentos, em gerarChamada.
 *
 * Retorno: tipo do fator. Literais tem tipo fixo (texto e caractere),
 *          variavel tem o tipo da minitabela e chamada de funcao tem o
 *          tipo de retorno declarado.
 *
 * O tipo e guardado numa variavel e devolvido num unico return no fim,
 * pelo mesmo motivo de tipo_basico: o compilador nao sabe que
 * erroSintatico nunca retorna.
 */
TokenNome fator(void)
{
      TokenNome tipo = TOKEN_INTEIRO;
      switch (lookahead.type)
      {
            case TOKEN_NUM_INT:
                  /* o valor e lido antes de avancar para o proximo token */
                  imprimirInteiro("CRCT", lookahead.attribute.int_value);
                  nextToken();
                  tipo = TOKEN_INTEIRO;
                  break;
            case TOKEN_NUM_FLOAT:
                  imprimirReal("CRCT", lookahead.attribute.float_value);
                  nextToken();
                  tipo = TOKEN_REAL;
                  break;
            case TOKEN_STRING:
                  /* a MEPA nao tem instrucao para texto: a cadeia nao gera
                   * codigo, e o escreva deixa de emitir o IMPR dela */
                  nextToken();
                  tipo = TOKEN_CARACTERE;
                  break;
            case TOKEN_VERDADEIRO:
                  imprimirInteiro("CRCT", 1);
                  nextToken();
                  tipo = TOKEN_LOGICO;
                  break;
            case TOKEN_FALSO:
                  imprimirInteiro("CRCT", 0);
                  nextToken();
                  tipo = TOKEN_LOGICO;
                  break;
            case TOKEN_SUB:
            {
                  /* menos unario: primeiro o operando, depois o INVR */
                  int linha = lookahead.line;
                  consumir(TOKEN_SUB);
                  tipo = fator();
                  if (!ehNumerico(tipo))
                        erroSemantico(linha, "menos unario exige operando numerico");
                  imprimir("INVR");
                  break;
            }
            case TOKEN_ABRE_PAR:
                  /* os parenteses so mudam a ordem das chamadas: nao geram
                   * instrucao propria */
                  consumir(TOKEN_ABRE_PAR);
                  tipo = expr();
                  consumir(TOKEN_FECHA_PAR);
                  break;
            case TOKEN_ID:
            {
                  /* chaves no case: uma declaracao logo depois de um rotulo
                   * nao compila em gcc anterior a versao 11 */
                  int idx = lookahead.attribute.table_index;
                  int linha = lookahead.line;
                  consumir(TOKEN_ID);
                  if (lookahead.type == TOKEN_ABRE_COL)
                  {
                        /* acesso a vetor: nomes[2] */
                        naoSuportado(linha, "vetores");
                        consumir(TOKEN_ABRE_COL);
                        expr();
                        consumir(TOKEN_FECHA_COL);
                  }
                  else if (lookahead.type == TOKEN_ABRE_PAR)
                  {
                        /* chamada de funcao: eh_par(num) */
                        Rotina *r = buscarRotina(idx);
                        if (r == NULL)
                              erroSemantico(linha, "identificador nao e uma funcao");
                        if (!r->ehFuncao)
                              erroSemantico(linha, "procedimento nao pode ser usado em expressao");

                        consumir(TOKEN_ABRE_PAR);
                        int numArgs = argumentos();
                        consumir(TOKEN_FECHA_PAR);
                        gerarChamada(r, numArgs, linha);

                        /* o retorno fica na posicao com o nome da funcao */
                        imprimirNome("CRVL", nomeDoSimbolo(r->nome));
                        tipo = r->tipoRetorno;
                  }
                  else
                  {
                        /* variavel simples: tem de ter sido declarada */
                        int endereco = busca_tabela_simbolos(nomeDoSimbolo(idx), linha);
                        usada[endereco] = 1;   /* regra 4 */
                        imprimirNome("CRVL", nomeDoSimbolo(idx));
                        tipo = tipoDaVariavel(endereco);
                  }
                  break;
            }
            default:
                  erroSintatico(lookahead, "expressao");
      }
      return tipo;
}

/*
 * conferirVariaveisNaoUsadas
 * ------------------------------------------------------------
 * Regra 4: toda variavel declarada e com valor atribuido deve ser
 * usada. Chamada no fim de programa(), quando todo o fonte ja foi lido.
 * Parametros nao contam como atribuidos (o valor vem de quem chama).
 *
 * O erro aponta a linha da declaracao e cita o nome da variavel.
 */
void conferirVariaveisNaoUsadas(void)
{
      for (TNo *atual = minitabela; atual != NULL; atual = atual->prox)
      {
            int e = atual->endereco;
            if (atribuida[e] && !usada[e])
            {
                  char mensagem[80];
                  snprintf(mensagem, sizeof mensagem,
                           "variavel %s recebe valor mas nunca e usada", atual->ID);
                  erroSemantico(linhaDecl[e], mensagem);
            }
      }
}

/*
 * programa -> ALGORITMO STRING declaracoes INICIO comandos FIMALGORITMO
 * ------------------------------------------------------------
 * Simbolo inicial da gramatica. O nome do algoritmo vem entre aspas,
 * por isso o segundo token e STRING e nao ID.
 *
 * Codigo gerado:
 *
 *        INPP
 *        DSVS Lp           <- pula o codigo das rotinas
 *        (rotinas)            gerado dentro de declaracoes()
 *    Lp: NADA
 *        AMEM n            <- n contado durante as declaracoes
 *        (comandos)
 *        PARA
 *
 * O AMEM fica depois de declaracoes() porque so ali todas as secoes
 * var ja foram lidas (elas podem aparecer depois de uma rotina). O
 * DSVS e emitido mesmo sem rotinas, para manter o codigo uniforme.
 */
void programa(void)
{
      imprimir("INPP");
      int rotuloPrincipal = proximo_rotulo();
      imprimirDesvio("DSVS", rotuloPrincipal);
      consumir(TOKEN_ALGORITMO);
      consumir(TOKEN_STRING);
      declaracoes();
      imprimirRotulo(rotuloPrincipal);
      imprimirInteiro("AMEM", totalPosicoes);
      consumir(TOKEN_INICIO);
      comandos();
      consumir(TOKEN_FIMALGORITMO);
      conferirVariaveisNaoUsadas();
      imprimir("PARA");
}

/*
 * declaracoes -> { secao_var | procedimento | funcao }
 * ------------------------------------------------------------
 * Aceita as tres formas em qualquer ordem e quantas vezes aparecerem,
 * ate encontrar um token que nao inicia nenhuma delas (normalmente o
 * INICIO do bloco principal).
 */
void declaracoes(void)
{
      while (1)
      {
            if (lookahead.type == TOKEN_VAR)
                  secao_var();
            else if (lookahead.type == TOKEN_PROCEDIMENTO)
                  procedimento();
            else if (lookahead.type == TOKEN_FUNCAO)
                  funcao();
            else
                  break;
      }
}

/*
 * secao_var -> VAR { decl_var }
 * ------------------------------------------------------------
 * A secao pode estar vazia (o anexo tem exemplos com "var" sem
 * nenhuma variavel embaixo), por isso o laco testa se ha um ID antes
 * de entrar.
 */
void secao_var(void)
{
      consumir(TOKEN_VAR);
      while (lookahead.type == TOKEN_ID)
            decl_var();
}

/*
 * registrarVar
 * ------------------------------------------------------------
 * Coloca uma variavel (ou parametro) na minitabela, conferindo antes
 * que o nome nao e de uma rotina. A conferencia contra outras variaveis
 * e feita dentro de inserirVarSemant (regra 3: nome unico).
 *
 * Usada por decl_var e por parametro, para que a regra fique escrita
 * num lugar so.
 *
 * Parametros: idx        - indice do nome na tabelaSimbolos
 *             tipoBasico - tipo basico declarado (TOKEN_INTEIRO, ...)
 *             linha      - linha do nome, para as mensagens de erro
 */
void registrarVar(int idx, TokenNome tipoBasico, int linha)
{
      if (buscarRotina(idx) != NULL)
            erroSemantico(linha, "nome ja usado por uma rotina");
      inserirVarSemant(nomeDoSimbolo(idx), tipoBasico, linha);

      /* inserirVarSemant acabou de dar o endereco proxEndereco - 1 */
      linhaDecl[proxEndereco - 1] = linha;
}

/*
 * decl_var -> lista_ids ":" tipo
 * ------------------------------------------------------------
 * Uma linha de declaracao, como "nome, sobrenome: caractere".
 *
 * Como a lista de nomes vem antes do tipo, lista_ids guarda o indice e
 * a linha de cada nome nos vetores locais e devolve quantos leu; tipo
 * devolve quantas posicoes cada um ocupa, e o produto soma no
 * totalPosicoes, que vira o operando do AMEM.
 *
 * Verificacao semantica: regra 3 (nome unico), em registrarVar.
 */
void decl_var(void)
{
      int indices[MAX_IDS_DECL];
      int linhas[MAX_IDS_DECL];
      TokenNome tipoBasico;

      /* parte sintatica: lista de ids, ':' e o tipo */
      int contagem_ids = lista_ids(indices, linhas);
      consumir(TOKEN_DOIS_PONTOS);
      int posicoes = tipo(&tipoBasico);

      /* parte semantica: so agora o tipo e conhecido, entao cada nome
       * vai para a minitabela, com a linha dele para as mensagens */
      for (int i = 0; i < contagem_ids; i++)
            registrarVar(indices[i], tipoBasico, linhas[i]);

      totalPosicoes += contagem_ids * posicoes;
}

/*
 * registrarId
 * ------------------------------------------------------------
 * Guarda o indice e a linha do ID corrente na posicao contagem_ids
 * dos vetores e consome o ID. Usada por lista_ids, que precisa
 * guardar os nomes porque o tipo so aparece depois deles.
 *
 * O limite e conferido antes de gravar, para nunca escrever fora dos
 * vetores de decl_var.
 *
 * Parametros: indices      - indice de cada nome na tabelaSimbolos
 *             linhas       - linha de cada nome, para mensagens de erro
 *             contagem_ids - posicao onde o ID corrente sera guardado
 */
void registrarId(int indices[], int linhas[], int contagem_ids)
{
      if (contagem_ids >= MAX_IDS_DECL)
            erroSemantico(lookahead.line, "muitas variaveis numa mesma declaracao");

      /* indice e linha guardados ANTES de consumir o ID */
      indices[contagem_ids] = lookahead.attribute.table_index;
      linhas[contagem_ids] = lookahead.line;
      consumir(TOKEN_ID);
}

/*
 * lista_ids -> ID { "," ID }
 * ------------------------------------------------------------
 * Um ou mais identificadores separados por virgula. Cada nome e
 * guardado (indice e linha) para que decl_var possa inserir todos na
 * minitabela depois de ler o tipo.
 *
 * Parametros: indices - recebe o indice de cada nome na tabelaSimbolos
 *             linhas  - recebe a linha de cada nome
 * Retorno:    quantidade de identificadores lidos.
 */
int lista_ids(int indices[], int linhas[])
{
      int contagem_ids = 0;

      registrarId(indices, linhas, contagem_ids);
      contagem_ids++;

      while (lookahead.type == TOKEN_VIRGULA)
      {
            consumir(TOKEN_VIRGULA);
            registrarId(indices, linhas, contagem_ids);
            contagem_ids++;
      }

      return contagem_ids;
}

/*
 * tipo -> tipo_basico | VETOR "[" NUM_INT ".." NUM_INT "]" DE tipo_basico
 * ------------------------------------------------------------
 * Se comecar com VETOR, consome a faixa e o DE; nos dois casos o tipo
 * termina num tipo basico, entao a chamada final e comum as duas
 * producoes.
 *
 * Os limites do vetor sao lidos do atributo do token ANTES de cada
 * consumir, porque depois o lookahead ja e o ".." ou o "]".
 *
 * O tipo basico lido e devolvido pelo ponteiro tipoBasico, porque o
 * retorno ja e usado pelas posicoes. Num vetor, e o tipo do elemento.
 *
 * Parametro: tipoBasico - recebe o tipo basico lido (TOKEN_INTEIRO,
 *                         TOKEN_REAL, TOKEN_CARACTERE ou TOKEN_LOGICO)
 * Retorno:   posicoes de memoria ocupadas por variavel deste tipo: 1 para
 *            tipo basico e fim - inicio + 1 para vetor (o +1 conta as
 *            duas pontas: [1..4] tem 4 posicoes).
 */
int tipo(TokenNome *tipoBasico)
{
      int num_posicoes = 1;
      if (lookahead.type == TOKEN_VETOR)
      {
            consumir(TOKEN_VETOR);
            consumir(TOKEN_ABRE_COL);
            int inicio = lookahead.attribute.int_value;
            consumir(TOKEN_NUM_INT);
            consumir(TOKEN_PONTO_PONTO);
            int fim = lookahead.attribute.int_value;
            consumir(TOKEN_NUM_INT);
            consumir(TOKEN_FECHA_COL);
            consumir(TOKEN_DE);
            num_posicoes = fim - inicio + 1;
      }
      *tipoBasico = tipo_basico();
      return num_posicoes;
}

/*
 * tipo_basico -> INTEIRO | REAL | CARACTERE | LOGICO
 * ------------------------------------------------------------
 * Os quatro tipos primitivos do MiniVisualg.
 *
 * O tipo e guardado no inicio e devolvido num unico return no fim:
 * como o compilador nao sabe que erroSintatico nunca retorna, um return
 * so dentro dos cases geraria o warning de funcao sem retorno.
 *
 * Retorno: o tipo lido (TOKEN_INTEIRO, TOKEN_REAL, TOKEN_CARACTERE ou
 *          TOKEN_LOGICO), usado nos parametros e no retorno de funcoes.
 */
TokenNome tipo_basico(void)
{
      TokenNome tipoLido = lookahead.type;
      switch (lookahead.type)
      {
            case TOKEN_INTEIRO:
            case TOKEN_REAL:
            case TOKEN_CARACTERE:
            case TOKEN_LOGICO:
                  nextToken();
                  break;
            default:
                  erroSintatico(lookahead, "tipo (inteiro, real, caractere ou logico)");
      }
      return tipoLido;
}

/*
 * iniciaComando
 * ------------------------------------------------------------
 * Conjunto FIRST de 'comando': diz se um token pode iniciar um
 * comando. E o que permite a comandos() saber onde a lista termina,
 * sem precisar conhecer o token que vem depois do bloco.
 *
 * Parametro: t - tipo do token a testar
 * Retorno:   1 se o token inicia um comando, 0 caso contrario.
 */
int iniciaComando(TokenNome t)
{
      return t == TOKEN_ID || t == TOKEN_SE || t == TOKEN_PARA
             || t == TOKEN_ENQUANTO || t == TOKEN_LEIA || t == TOKEN_ESCREVA
             || t == TOKEN_ESCREVAL || t == TOKEN_RETORNE;
}

/*
 * comandos -> { comando }
 * ------------------------------------------------------------
 * Sequencia de comandos, possivelmente vazia (um "se" sem nenhum
 * comando dentro e sintaticamente valido).
 */
void comandos(void)
{
      while (iniciaComando(lookahead.type))
            comando();
}

/*
 * comando -> cmd_id | cmd_se | cmd_para | cmd_enquanto
 *          | cmd_leia | cmd_escreva | cmd_retorne
 * ------------------------------------------------------------
 * Escolhe a producao pelo token atual. Cada comando tem um token
 * inicial proprio, por isso um unico lookahead basta.
 */
void comando(void)
{
      switch (lookahead.type)
      {
            case TOKEN_ID:       cmd_id();       break;
            case TOKEN_SE:       cmd_se();       break;
            case TOKEN_PARA:     cmd_para();     break;
            case TOKEN_ENQUANTO: cmd_enquanto(); break;
            case TOKEN_LEIA:     cmd_leia();     break;
            case TOKEN_ESCREVA:
            case TOKEN_ESCREVAL: cmd_escreva();  break;
            case TOKEN_RETORNE:  cmd_retorne();  break;
            default:             erroSintatico(lookahead, "comando");
      }
}

/*
 * cmd_escreva -> ( ESCREVA | ESCREVAL ) "(" argumentos ")"
 * ------------------------------------------------------------
 * Os dois comandos tem a mesma sintaxe e so diferem na quebra de
 * linha, que e questao de execucao e nao de analise. Por isso a
 * mesma funcao atende aos dois, e o codigo gerado e o mesmo (a MEPA
 * nao tem instrucao de quebra de linha).
 *
 * Codigo gerado: para cada argumento, <expr> IMPR.
 *
 * O laco e proprio, em vez de argumentos(), por dois motivos: o IMPR
 * vai depois de cada argumento, e argumentos() tambem serve as chamadas
 * de rotina, onde nao pode haver IMPR. Um texto entre aspas nao empilha
 * nada (ver fator), entao o argumento que comeca com STRING nao recebe
 * IMPR. Limitacao: concatenacao como "a" + x geraria codigo errado.
 */
void cmd_escreva(void)
{
      nextToken();   /* comando() ja conferiu que e ESCREVA ou ESCREVAL */
      consumir(TOKEN_ABRE_PAR);

      do
      {
            if (lookahead.type == TOKEN_VIRGULA)
                  consumir(TOKEN_VIRGULA);

            int ehTexto = (lookahead.type == TOKEN_STRING);
            expr();
            if (!ehTexto)
                  imprimir("IMPR");
      } while (lookahead.type == TOKEN_VIRGULA);

      consumir(TOKEN_FECHA_PAR);
}

/*
 * cmd_id   -> ID resto_id
 * resto_id -> "<-" expr | "[" expr "]" "<-" expr | "(" argumentos ")"
 *           | vazio
 * ------------------------------------------------------------
 * Um comando que comeca com identificador pode ser uma atribuicao,
 * uma atribuicao a elemento de vetor ou a chamada de um procedimento.
 * So da para decidir depois de consumir o ID e olhar o proximo token,
 * por isso o indice e a linha do ID sao guardados antes.
 *
 * O caso vazio cobre a chamada de procedimento sem parametros, que no
 * Visualg e escrita sem parenteses (linha_decorativa).
 *
 * Codigo gerado:
 *    x <- expr .......... <expr> ARMZ x
 *    p(args) / p ........ <args> ARMZ params CHPR Lp
 *    v[i] <- expr ....... nao suportado (vetores)
 *
 * Verificacoes semanticas: regra 1 (na atribuicao, a variavel de destino
 * tem de ter sido declarada, via busca_tabela_simbolos), regra 5 (o tipo
 * da expressao tem de caber no da variavel, via tipoAtribuivel), regra 8 (funcao
 * nao pode ser usada como procedimento) e identificador que nao e
 * procedimento usado como comando.
 */
void cmd_id(void)
{
      int idx = lookahead.attribute.table_index;
      int linha = lookahead.line;
      consumir(TOKEN_ID);
      if (lookahead.type == TOKEN_ATRIB)
      {
            consumir(TOKEN_ATRIB);
            /* o destino e conferido antes da expressao: em "y <- z" com os
             * dois sem declaracao, o erro aponta o y, que vem primeiro */
            int endereco = busca_tabela_simbolos(nomeDoSimbolo(idx), linha);
            atribuida[endereco] = 1;   /* regra 4 */
            TokenNome tipoExpr = expr();

            /* regra 5: o valor tem de caber no tipo da variavel */
            if (!tipoAtribuivel(tipoDaVariavel(endereco), tipoExpr))
                  erroSemantico(linha, "tipo incompativel na atribuicao");

            imprimirNome("ARMZ", nomeDoSimbolo(idx));
      }
      else if (lookahead.type == TOKEN_ABRE_COL)
      {
            naoSuportado(linha, "vetores");
            consumir(TOKEN_ABRE_COL);
            expr();
            consumir(TOKEN_FECHA_COL);
            consumir(TOKEN_ATRIB);
            expr();
      }
      else if (lookahead.type == TOKEN_ABRE_PAR)
      {
            Rotina *r = buscarRotina(idx);
            if (r == NULL)
                  erroSemantico(linha, "identificador nao e um procedimento");
            if (r->ehFuncao)
                  erroSemantico(linha, "funcao nao pode ser usada como procedimento");

            consumir(TOKEN_ABRE_PAR);
            int numArgs = argumentos();
            consumir(TOKEN_FECHA_PAR);
            gerarChamada(r, numArgs, linha);
      }
      else
      {
            Rotina *r = buscarRotina(idx);
            if (r == NULL)
                  erroSemantico(linha, "identificador nao e um procedimento");
            if (r->ehFuncao)
                  erroSemantico(linha, "funcao nao pode ser usada como procedimento");

            gerarChamada(r, 0, linha);
      }
}

/*
 * cmd_leia -> LEIA "(" variavel ")"
 * ------------------------------------------------------------
 * O argumento do leia tem de ser uma variavel (ou posicao de vetor),
 * nao uma expressao qualquer: ler para dentro de "a + b" nao faz
 * sentido. Por isso chama variavel() e nao expr().
 *
 * Codigo gerado: LEIT, ARMZ variavel.
 */
void cmd_leia(void)
{
      consumir(TOKEN_LEIA);
      consumir(TOKEN_ABRE_PAR);
      int idx = variavel();
      imprimir("LEIT");
      imprimirNome("ARMZ", nomeDoSimbolo(idx));
      consumir(TOKEN_FECHA_PAR);
}

/*
 * variavel -> ID [ "[" expr "]" ]
 * ------------------------------------------------------------
 * Variavel simples ou elemento de vetor, como notas[i]. Nao gera codigo:
 * so reconhece a variavel e devolve o nome para quem chamou decidir o
 * que emitir. O elemento de vetor nao e suportado na geracao.
 *
 * Verificacao semantica: regra 1 (variavel declarada antes do uso, via
 * busca_tabela_simbolos).
 *
 * Retorno: indice do nome da variavel na tabelaSimbolos.
 */
int variavel(void)
{
      int idx = lookahead.attribute.table_index;
      int linha = lookahead.line;
      consumir(TOKEN_ID);
      int endereco = busca_tabela_simbolos(nomeDoSimbolo(idx), linha);
      atribuida[endereco] = 1;   /* regra 4: variavel() so e usada pelo leia */
      if (lookahead.type == TOKEN_ABRE_COL)
      {
            naoSuportado(linha, "vetores");
            consumir(TOKEN_ABRE_COL);
            expr();
            consumir(TOKEN_FECHA_COL);
      }
      return idx;
}

/*
 * cmd_se -> SE expr ENTAO comandos [ SENAO comandos ] FIMSE
 * ------------------------------------------------------------
 * O FIMSE obrigatorio elimina a ambiguidade do "senao pendurado": em
 * um se dentro de outro, cada senao pertence ao se que ainda nao foi
 * fechado.
 *
 * Codigo gerado (com senao):         (sem senao):
 *
 *        <expr>                            <expr>
 *        DSVF Ls                           DSVF Ls
 *        <entao>                           <entao>
 *        DSVS Lf                       Ls: NADA
 *    Ls: NADA
 *        <senao>
 *    Lf: NADA
 *
 * Os dois rotulos sao pedidos no inicio e ficam em variaveis locais
 * (ver proximo_rotulo). Sem senao o segundo nao e usado e a numeracao
 * fica com uma lacuna, o que nao afeta a execucao.
 */
void cmd_se(void)
{
      int rotuloSenao = proximo_rotulo();
      int rotuloFim = proximo_rotulo();

      consumir(TOKEN_SE);
      expr();
      imprimirDesvio("DSVF", rotuloSenao);
      consumir(TOKEN_ENTAO);
      comandos();
      if (lookahead.type == TOKEN_SENAO)
      {
            imprimirDesvio("DSVS", rotuloFim);
            imprimirRotulo(rotuloSenao);
            consumir(TOKEN_SENAO);
            comandos();
            imprimirRotulo(rotuloFim);
      }
      else
      {
            /* sem senao, o rotulo do senao e o proprio fim do se */
            imprimirRotulo(rotuloSenao);
      }

      consumir(TOKEN_FIMSE);
}

/*
 * cmd_para -> PARA ID DE expr ATE expr [ PASSO expr ] FACA
 *             comandos FIMPARA
 * ------------------------------------------------------------
 * O PASSO e opcional; quando aparece, o valor pode ser negativo, como
 * em "para i de 10 ate 0 passo -2 faca" (o lexico entrega "-2" como
 * SUB seguido de NUM_INT).
 *
 * Codigo gerado:
 *
 *        <inicio>
 *        ARMZ i
 *    Li: NADA
 *        CRVL i
 *        <limite>
 *        CMEG | CMAG       <- CMEG se o passo e positivo, CMAG se negativo
 *        DSVF Lf
 *        <corpo>
 *        CRVL i
 *        CRCT passo
 *        SOMA
 *        ARMZ i            <- incremento gerado pelo compilador
 *        DSVS Li
 *    Lf: NADA
 *
 * No fonte o passo aparece ANTES do corpo, mas o incremento precisa
 * sair DEPOIS dele. Como o codigo e gerado durante a leitura, o passo e
 * restrito a uma constante inteira, lida direto do token e guardada
 * numa variavel local (sem gerar codigo na hora). Pelo mesmo motivo a
 * comparacao so e emitida depois de ler o passo: e o sinal dele que
 * decide entre CMEG e CMAG.
 *
 * Verificacoes semanticas: regra 1 (variavel de controle declarada antes
 * do uso, via busca_tabela_simbolos), passo nao constante e passo zero
 * (que faria o laco nunca terminar).
 */
void cmd_para(void)
{
      int rotuloInicio = proximo_rotulo();
      int rotuloFim = proximo_rotulo();

      consumir(TOKEN_PARA);
      int idx = lookahead.attribute.table_index;
      int linha = lookahead.line;
      consumir(TOKEN_ID);
      int endereco = busca_tabela_simbolos(nomeDoSimbolo(idx), linha);

      /* regra 4: o laco atribui a variavel e tambem a le na comparacao */
      atribuida[endereco] = 1;
      usada[endereco] = 1;

      consumir(TOKEN_DE);
      expr();
      imprimirNome("ARMZ", nomeDoSimbolo(idx));

      consumir(TOKEN_ATE);
      imprimirRotulo(rotuloInicio);
      imprimirNome("CRVL", nomeDoSimbolo(idx));
      expr();

      int passo = 1;
      if (lookahead.type == TOKEN_PASSO)
      {
            consumir(TOKEN_PASSO);
            int linhaPasso = lookahead.line;

            int negativo = 0;
            if (lookahead.type == TOKEN_SUB)
            {
                  consumir(TOKEN_SUB);
                  negativo = 1;
            }

            if (lookahead.type != TOKEN_NUM_INT)
                  erroSemantico(linhaPasso, "passo do para deve ser uma constante inteira");

            passo = lookahead.attribute.int_value;
            consumir(TOKEN_NUM_INT);

            if (negativo)
                  passo = -passo;

            if (passo == 0)
                  erroSemantico(linhaPasso, "passo do para nao pode ser zero");
      }

      if (passo > 0)
            imprimir("CMEG");
      else
            imprimir("CMAG");
      imprimirDesvio("DSVF", rotuloFim);

      consumir(TOKEN_FACA);
      comandos();

      imprimirNome("CRVL", nomeDoSimbolo(idx));
      imprimirInteiro("CRCT", passo);
      imprimir("SOMA");
      imprimirNome("ARMZ", nomeDoSimbolo(idx));

      imprimirDesvio("DSVS", rotuloInicio);
      imprimirRotulo(rotuloFim);

      consumir(TOKEN_FIMPARA);
}

/*
 * cmd_enquanto -> ENQUANTO expr FACA comandos FIMENQUANTO
 * ------------------------------------------------------------
 * Laco condicional.
 *
 * Codigo gerado:
 *
 *    Li: NADA              <- antes da condicao: ela e recalculada a
 *        <expr>               cada volta
 *        DSVF Lf
 *        <corpo>
 *        DSVS Li
 *    Lf: NADA
 */
void cmd_enquanto(void)
{
      int rotuloInicio = proximo_rotulo();
      int rotuloFim = proximo_rotulo();

      consumir(TOKEN_ENQUANTO);
      imprimirRotulo(rotuloInicio);
      expr();
      imprimirDesvio("DSVF", rotuloFim);
      consumir(TOKEN_FACA);
      comandos();
      imprimirDesvio("DSVS", rotuloInicio);
      imprimirRotulo(rotuloFim);
      consumir(TOKEN_FIMENQUANTO);
}

/*
 * cmd_retorne -> RETORNE expr
 * ------------------------------------------------------------
 * Retorno de funcao. O valor calculado e guardado na posicao de memoria
 * com o nome da funcao (quem chamou le dali com CRVL depois do CHPR),
 * e o RTPR sai da funcao na hora, mesmo que o retorne esteja no meio do
 * corpo (dentro de um se, por exemplo).
 *
 * Codigo gerado: <expr> ARMZ nome_da_funcao RTPR 1,0
 *
 * So faz sentido dentro de uma funcao: no programa principal ou num
 * procedimento nao existe onde guardar o valor, entao e erro semantico.
 * A conferencia do tipo do valor (regra 9) usa rotinaAtual->tipoRetorno.
 */
void cmd_retorne(void)
{
      int linha = lookahead.line;
      consumir(TOKEN_RETORNE);

      if (rotinaAtual == NULL || !rotinaAtual->ehFuncao)
            erroSemantico(linha, "retorne so pode ser usado dentro de uma funcao");

      expr();
      imprimirNome("ARMZ", nomeDoSimbolo(rotinaAtual->nome));
      imprimirDoisInteiros("RTPR", 1, 0);
}

/*
 * procedimento -> PROCEDIMENTO ID [ "(" parametros ")" ]
 *                 INICIO comandos FIMPROCEDIMENTO
 * ------------------------------------------------------------
 * A lista de parametros e opcional: o anexo diz que procedimentos sem
 * parametros nao levam parenteses nem na declaracao nem na chamada.
 *
 * Codigo gerado (entre o DSVS e o AMEM do programa principal):
 *
 *    Lp: NADA              <- rotulo guardado na tabela, usado pelo CHPR
 *        ENPR 1
 *        <corpo>
 *        RTPR 1,0
 *
 * ENPR e RTPR sao as instrucoes de entrada e saida de rotina da MEPA do
 * livro de Kowaltowski (nao aparecem nos slides; ver README). O nivel
 * lexico e sempre 1 e o RTPR descarta 0 posicoes, porque os parametros
 * ficam em posicoes de memoria com nome, e nao na pilha.
 *
 * A rotina e registrada antes do corpo, para que o nome ja seja
 * conhecido, e rotinaAtual marca que o corpo esta dentro dela.
 *
 * Verificacao semantica: regra 3 (o nome nao pode repetir o de outra
 * rotina nem o de uma variavel ja declarada).
 */
void procedimento(void)
{
      consumir(TOKEN_PROCEDIMENTO);
      int idx = lookahead.attribute.table_index;
      int linha = lookahead.line;
      consumir(TOKEN_ID);

      /* regra 3: o nome nao pode repetir o de outra rotina nem o de uma
       * variavel. Conferido antes de inserir, senao a rotina se acharia */
      if (buscarRotina(idx) != NULL)
            erroSemantico(linha, "rotina declarada mais de uma vez");
      if (existeNaMinitabela(nomeDoSimbolo(idx)))
            erroSemantico(linha, "nome ja usado por uma variavel");

      Rotina *r = inserirRotina(idx, 0);
      r->rotulo = proximo_rotulo();
      rotinaAtual = r;

      imprimirRotulo(r->rotulo);
      imprimirInteiro("ENPR", 1);

      if (lookahead.type == TOKEN_ABRE_PAR)
      {
            consumir(TOKEN_ABRE_PAR);
            parametros(r);
            consumir(TOKEN_FECHA_PAR);
      }
      consumir(TOKEN_INICIO);
      comandos();

      imprimirDoisInteiros("RTPR", 1, 0);
      rotinaAtual = NULL;

      consumir(TOKEN_FIMPROCEDIMENTO);
}

/*
 * funcao -> FUNCAO ID "(" parametros ")" ":" tipo_basico
 *           INICIO comandos FIMFUNCAO
 * ------------------------------------------------------------
 * Diferente do procedimento, a funcao sempre declara parenteses e o
 * tipo de retorno depois dos dois pontos.
 *
 * Gera o mesmo codigo do procedimento. A diferenca e que a funcao
 * reserva uma posicao de memoria a mais, com o proprio nome, onde o
 * retorne guarda o valor de retorno.
 *
 * Verificacao semantica: regra 3, a mesma do procedimento.
 */
void funcao(void)
{
      consumir(TOKEN_FUNCAO);
      int idx = lookahead.attribute.table_index;
      int linha = lookahead.line;
      consumir(TOKEN_ID);

      /* regra 3: mesma conferencia do procedimento */
      if (buscarRotina(idx) != NULL)
            erroSemantico(linha, "rotina declarada mais de uma vez");
      if (existeNaMinitabela(nomeDoSimbolo(idx)))
            erroSemantico(linha, "nome ja usado por uma variavel");

      Rotina *r = inserirRotina(idx, 1);
      r->rotulo = proximo_rotulo();
      rotinaAtual = r;

      totalPosicoes++;

      imprimirRotulo(r->rotulo);
      imprimirInteiro("ENPR", 1);

      consumir(TOKEN_ABRE_PAR);
      parametros(r);
      consumir(TOKEN_FECHA_PAR);
      consumir(TOKEN_DOIS_PONTOS);
      r->tipoRetorno = tipo_basico();

      consumir(TOKEN_INICIO);
      comandos();

      imprimirDoisInteiros("RTPR", 1, 0);
      rotinaAtual = NULL;

      consumir(TOKEN_FIMFUNCAO);
}

/*
 * parametros -> parametro { "," parametro }
 * ------------------------------------------------------------
 * Lista de parametros formais separados por virgula.
 *
 * Parametro: r - rotina que esta sendo declarada, repassada a cada
 *                parametro()
 */
void parametros(Rotina *r)
{
      parametro(r);
      while (lookahead.type == TOKEN_VIRGULA)
      {
            consumir(TOKEN_VIRGULA);
            parametro(r);
      }
}

/*
 * parametro -> ID ":" tipo_basico
 * ------------------------------------------------------------
 * Um parametro formal, como "a: inteiro". Registra o nome e o tipo na
 * rotina (na ordem da declaracao) e reserva uma posicao de memoria para
 * ele no AMEM: o parametro e uma posicao com nome, como uma variavel.
 *
 * Pelo mesmo motivo o parametro tambem entra na minitabela, para que o
 * seu uso dentro da rotina passe pela verificacao de declaracao. Como
 * todos os nomes sao globais, dois parametros com o mesmo nome (mesmo
 * em rotinas diferentes) sao erro de nome repetido.
 *
 * Verificacao semantica: regra 3 (o nome nao pode repetir o de uma
 * rotina nem o de outra variavel ou parametro).
 *
 * Parametro: r - rotina a que o parametro pertence
 */
void parametro(Rotina *r)
{
      int idx = lookahead.attribute.table_index;
      int linha = lookahead.line;
      consumir(TOKEN_ID);
      consumir(TOKEN_DOIS_PONTOS);
      TokenNome tipoParam = tipo_basico();
      adicionarParametro(r, idx, tipoParam);
      registrarVar(idx, tipoParam, linha);
      totalPosicoes++;
}

/* ============================================================
 *  8. MAIN
 * ============================================================ */

/*
 * main
 * ------------------------------------------------------------
 * Recebe o nome do fonte por linha de comando, carrega o arquivo,
 * abre as duas saidas e dispara a compilacao.
 *
 * O primeiro nextToken() carrega o lookahead inicial (o parser sempre
 * precisa de um token na mao antes de comecar). Depois de programa(),
 * o teste de TOKEN_EOF garante que nao sobrou nada depois do
 * fimalgoritmo.
 *
 * Retorno: 0 se a compilacao terminou sem erros, 1 em caso de erro de
 *          uso ou de arquivo. Os erros lexicos, sintaticos, semanticos
 *          e de recurso nao suportado encerram o programa antes, dentro
 *          das proprias funcoes de erro.
 */
int main(int argc, char *argv[])
{
      if (argc < 2)
      {
            printf("Uso: %s <arquivo_fonte.alg>\n", argv[0]);
            return 1;
      }

      if (!carregarArquivo(argv[1]))
      {
            printf("Erro ao abrir o arquivo %s\n", argv[1]);
            return 1;
      }

      arquivoSaida = fopen(ARQUIVO_SAIDA, "w");
      if (arquivoSaida == NULL)
      {
            printf("Erro ao criar %s\n", ARQUIVO_SAIDA);
            free(buffer);
            return 1;
      }

      arquivoMepa = fopen(ARQUIVO_MEPA, "w");
      if (arquivoMepa == NULL)
      {
            printf("Erro ao criar %s\n", ARQUIVO_MEPA);
            fclose(arquivoSaida);
            free(buffer);
            return 1;
      }

      /* Todas as etapas numa unica passagem: o parser pede os tokens ao
       * lexico, que imprime cada um deles, e emite o codigo MEPA
       * enquanto a analise acontece. */
      nextToken();
      programa();

      if (lookahead.type != TOKEN_EOF)
            erroSintatico(lookahead, "fim do arquivo");

      printf("Analise sintatica concluida sem erros.\n");
      fprintf(arquivoSaida, "Analise sintatica concluida sem erros.\n");

      fclose(arquivoSaida);
      fclose(arquivoMepa);
      free(buffer);
      liberarTabelaSimbolos();
      return 0;
}
