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
 *    saida_tokens.txt .. tokens e resultado (tambem na tela)
 *    saida_mepa.txt .... codigo MEPA gerado; apagado em qualquer erro
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
 *    5. Analisador lexico ....... Etapa 2 (obter_token)
 *    6. Geracao de codigo ....... funcoes de emissao MEPA, rotulos e
 *                                 montagem de chamadas
 *    7. Analisador sintatico .... Etapa 3 (next_token e a gramatica),
 *                                 que tambem dirige a geracao de codigo
 *    8. main .................... liga tudo
 *
 *  FLUXO DE EXECUCAO (Figura 1 do enunciado)
 *
 *    O parser dirige tudo numa unica leitura do fonte: pede cada token
 *    com next_token(), que chama o lexico e ja registra o token na
 *    listagem. Ao reconhecer cada construcao, o parser faz as
 *    verificacoes semanticas e emite o codigo MEPA correspondente (sem
 *    arvore intermediaria nem segunda passada).
 *
 *      main -> next_token -> obter_token -> reconhecer_token
 *                               |
 *                               +-> imprimir_token -> info_token
 *
 *      cmd_se -> expr -> ... -> fator -> imprimir_nome("CRVL", ...)
 *             -> imprimir_desvio("DSVF", ...) -> comandos -> ...
 *
 *  Termina com codigo 0 quando a compilacao vai ate o fim e com 1 em
 *  qualquer erro (arquivo, lexico, sintatico, semantico ou recurso nao
 *  suportado).
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
 * Os cinco primeiros sao os do enunciado. Os demais foram acrescentados
 * com este criterio: lexemas que aparecem no mesmo ponto da gramatica
 * viram um token com atributo (como TOKEN_OP_REL, com o operador em
 * op_code); lexemas que levam a producoes diferentes viram tokens
 * diferentes. Por isso cada palavra reservada tem o seu token, em vez de
 * um TOKEN_KEYWORD generico.
 *
 * TOKEN_TOTAL nao e um token: e um sentinela com a quantidade de valores
 * do enum, usado para validar o tamanho de nomes_tokens.
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
 * Nome textual de cada token, usado na listagem e nas mensagens de erro.
 * Indexada pelo proprio enum (inicializadores designados), entao a ordem
 * das linhas nao importa.
 */
static const char *nomes_tokens[] = {
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

/* Se este assert quebrar, faltou ou sobrou um nome na tabela acima. */
_Static_assert(sizeof(nomes_tokens) / sizeof(nomes_tokens[0]) == TOKEN_TOTAL,
               "nomes_tokens dessincronizado com o enum TokenNome");

/*
 * Sub-codigos do atributo dos operadores relacionais.
 *
 * Todos aparecem no mesmo ponto da gramatica (expr_rel), entao o token e
 * sempre TOKEN_OP_REL e o operador exato fica em attribute.op_code, como
 * sugere a Figura 2. OP_NE foi acrescentado porque "<>" aparece nos
 * exemplos do anexo.
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
 * Indexada pelo proprio enum, mesma ideia de nomes_tokens. */
static const char *nomes_op_rel[] = {
      [OP_LT] = "LT",
      [OP_LE] = "LE",
      [OP_EQ] = "EQ",
      [OP_GT] = "GT",
      [OP_GE] = "GE",
      [OP_NE] = "NE"
};

_Static_assert(sizeof(nomes_op_rel) / sizeof(nomes_op_rel[0]) == OP_TOTAL,
               "nomes_op_rel dessincronizado com o enum OpRelType");

/* Instrucao MEPA de comparacao de cada operador relacional, usada em
 * expr_rel. Indexada pelo enum, como nomes_op_rel. */
static const char *instrucoes_op_rel[] = {
      [OP_LT] = "CMME",
      [OP_LE] = "CMEG",
      [OP_EQ] = "CMIG",
      [OP_GT] = "CMMA",
      [OP_GE] = "CMAG",
      [OP_NE] = "CMDG"
};

_Static_assert(sizeof(instrucoes_op_rel) / sizeof(instrucoes_op_rel[0]) == OP_TOTAL,
               "instrucoes_op_rel dessincronizado com o enum OpRelType");

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
 * O campo 'line' e a linha do fonte, usada na listagem e nos erros.
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
 * A minitabela (TNo) guarda so variaveis, entao as rotinas ficam numa
 * tabela propria. A analise semantica usa os tipos para conferir
 * chamadas e retornos; a geracao de codigo usa o rotulo e os nomes dos
 * parametros para montar a chamada (CHPR). Nomes sao indices na
 * tabela_simbolos, e tipos usam o enum de tokens (TOKEN_INTEIRO...).
 */
typedef struct {
      int nome;                         /* indice do nome na tabela_simbolos */
      int ehFuncao;                     /* 1 = funcao, 0 = procedimento     */
      int rotulo;                       /* rotulo do inicio do codigo       */
      int numParams;                    /* quantidade de parametros         */
      int params[MAX_PARAMS];           /* indice de cada parametro, em ordem */
      TokenNome tiposParams[MAX_PARAMS];/* tipo de cada parametro           */
      TokenNome tipoRetorno;            /* tipo de retorno (so funcao)      */
      int temRetorne;                   /* 1 se o corpo tem algum retorne   */
} Rotina;

/* Par lexema -> token usado na tabela de palavras reservadas. */
typedef struct {
      const char *lexema;
      TokenNome token;
} PalavraReservada;

/*
 * Tabela de palavras reservadas do MiniVisualg.
 *
 * O lexico reconhece letras, digitos e '_' como identificador e so
 * depois consulta esta tabela. A comparacao ignora maiusculas, porque o
 * anexo escreve "MOD" e "E" em maiusculo e o resto em minusculo.
 */
const PalavraReservada palavras_reservadas[] = {
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
 * A minitabela guarda as variaveis declaradas (e os parametros) e diz se
 * um nome foi declarado, com qual endereco e tipo. E diferente da
 * tabela_simbolos do lexico, que guarda todo identificador do fonte,
 * declarado ou nao. Lista encadeada com insercao sempre no FIM e sem
 * remocao: a posicao do no na lista e o seu endereco.
 *
 * Campos:
 *   ID       - nome da variavel (no maximo 15 caracteres + '\0')
 *   endereco - ordem de declaracao: 0, 1, 2...
 *   tipo     - "int", "real", "char" ou "logico": "inteiro" e
 *              "caractere" nao cabem nas 7 posicoes do campo
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
 *  O fonte inteiro fica em memoria e o lexico caminha sobre ele com um
 *  indice, o que facilita olhar o proximo caractere (para decidir entre
 *  "<", "<=", "<>" e "<-", por exemplo).
 * ============================================================ */

static char *buffer = NULL;          /* codigo fonte inteiro em memoria */
static int posicao = 0;              /* posicao atual no buffer         */
static int linha_atual = 1;          /* linha atual do fonte            */
static FILE *arquivo_saida = NULL;   /* saida_tokens.txt                */
static FILE *arquivo_mepa = NULL;    /* saida_mepa.txt                  */

static Token lookahead;              /* token atual visto pelo parser   */

/* Tabela de simbolos simples: guarda os lexemas de identificadores e
 * de cadeias. O atributo table_index do token e o indice aqui dentro. */
static char tabela_simbolos[MAX_SIMBOLOS][MAX_LEXEMA];
static int total_simbolos = 0;

/* Posicoes reservadas pelo AMEM: 1 por variavel simples, o tamanho de
 * cada vetor, 1 por parametro e mais 1 por funcao (valor de retorno). */
static int total_posicoes = 0;

/* Tabela de rotinas (procedimentos e funcoes), na ordem de declaracao. */
static Rotina tabela_rotinas[MAX_ROTINAS];
static int total_rotinas = 0;

/* Rotina cujo corpo esta sendo analisado (NULL no programa principal).
 * Usada por cmd_retorne para saber em qual funcao guardar o valor. */
static Rotina *rotina_atual = NULL;

/* Quantos textos ("abc") o parser ja leu em expressoes. Texto nao gera
 * codigo, o que so e correto no escreva (ver recusar_texto_sem_codigo). */
static int textos_sem_codigo = 0;

/* ------------------------------------------------------------
 *  Fase 2 - minitabela de simbolos (TNo)
 * ------------------------------------------------------------ */

/* Primeiro no da minitabela e endereco da proxima variavel declarada.
 * E um contador proprio: total_posicoes tambem conta vetores e funcoes
 * e serve so para o AMEM. */

static TNo *minitabela = NULL;
static int prox_endereco = 0;

/* Regra 4 (variavel com valor atribuido tem de ser usada). As marcas
 * ficam fora da TNo porque o enunciado define a struct com exatamente
 * quatro campos; os vetores sao indexados pelo endereco da variavel.
 * Como cada variavel e um nome distinto da tabela_simbolos, o endereco
 * nunca passa de MAX_SIMBOLOS. Por serem globais, comecam zerados. */
static int atribuida[MAX_SIMBOLOS];   /* 1 se recebeu valor (<- ou leia)  */
static int usada[MAX_SIMBOLOS];       /* 1 se foi lida numa expressao     */
static int linha_decl[MAX_SIMBOLOS];  /* linha da declaracao, para o erro */

/* Prototipos: sair_com_erro e erro_semantico sao definidas na secao 4, mas
 * sao usadas antes dela pelas funcoes de tabela da secao 3. */
void sair_com_erro(void);
void erro_semantico(int linha, const char *mensagem);

/* ============================================================
 *  3. FUNCOES AUXILIARES
 * ============================================================ */

/*
 * carregar_arquivo
 * ------------------------------------------------------------
 * Le o fonte inteiro para o buffer global, terminado em '\0'.
 *
 * Abre em modo binario de proposito: em modo texto o MinGW converte
 * "\r\n" em "\n" e o tamanho dado por ftell deixa de bater com o lido.
 * O '\r' que sobra e descartado por pular_espacos_e_comentarios.
 *
 * Parametro: nome_arquivo - caminho do fonte .alg
 * Retorno:   1 em caso de sucesso, 0 se nao conseguiu abrir o arquivo
 *            ou alocar memoria.
 */
int carregar_arquivo(const char *nome_arquivo)
{
      FILE *arquivo = fopen(nome_arquivo, "rb");
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
 * inserir_simbolo
 * ------------------------------------------------------------
 * Procura o lexema na tabela de simbolos e, se nao existir, insere no
 * fim (busca linear, a tabela e pequena). O mesmo lexema sempre recebe
 * o mesmo indice.
 *
 * Parametro: lexema - texto ja terminado em '\0'
 * Retorno:   indice do lexema na tabela.
 */
int inserir_simbolo(const char *lexema)
{
      for (int i = 0; i < total_simbolos; i++)
      {
            if (strcmp(tabela_simbolos[i], lexema) == 0)
                  return i;
      }

      /* tabela cheia: para com mensagem em vez de escrever fora do vetor */
      if (total_simbolos >= MAX_SIMBOLOS)
      {
            printf("ERRO: tabela de simbolos cheia (limite de %d entradas)\n",
                   MAX_SIMBOLOS);
            if (arquivo_saida != NULL)
            {
                  fprintf(arquivo_saida,
                          "ERRO: tabela de simbolos cheia (limite de %d entradas)\n",
                          MAX_SIMBOLOS);
            }
            sair_com_erro();
      }

      strncpy(tabela_simbolos[total_simbolos], lexema, MAX_LEXEMA - 1);
      tabela_simbolos[total_simbolos][MAX_LEXEMA - 1] = '\0';
      return total_simbolos++;
}

/*
 * inserir_rotina
 * ------------------------------------------------------------
 * Registra um procedimento ou funcao na tabela de rotinas, antes do
 * corpo, para que o nome ja seja conhecido se o corpo chamar a propria
 * rotina. O rotulo e o tipo de retorno sao preenchidos por quem chamou.
 *
 * Parametros: nome     - indice do nome na tabela_simbolos
 *             ehFuncao - 1 para funcao, 0 para procedimento
 * Retorno:    ponteiro para a rotina criada, para o parser completar os
 *             campos e acrescentar os parametros.
 */
Rotina *inserir_rotina(int nome, int ehFuncao)
{
      if (total_rotinas >= MAX_ROTINAS)
      {
            printf("ERRO: tabela de rotinas cheia (limite de %d rotinas)\n",
                   MAX_ROTINAS);
            fprintf(arquivo_saida,
                    "ERRO: tabela de rotinas cheia (limite de %d rotinas)\n",
                    MAX_ROTINAS);
            sair_com_erro();
      }

      Rotina *r = &tabela_rotinas[total_rotinas];
      r->nome = nome;
      r->ehFuncao = ehFuncao;
      r->numParams = 0;
      r->temRetorne = 0;

      total_rotinas++;

      return r;
}

/*
 * adicionar_parametro
 * ------------------------------------------------------------
 * Acrescenta um parametro ao fim da lista da rotina, na ordem da
 * declaracao (a chamada depende dela para casar argumento e parametro).
 *
 * Parametros: r    - rotina que recebe o parametro
 *             nome - indice do nome do parametro na tabela_simbolos
 *             tipo - tipo declarado (TOKEN_INTEIRO, TOKEN_REAL, ...)
 */
void adicionar_parametro(Rotina *r, int nome, TokenNome tipo)
{
      if (r->numParams >= MAX_PARAMS)
      {
            printf("ERRO: rotina com parametros demais (limite de %d)\n",
                   MAX_PARAMS);
            fprintf(arquivo_saida,
                    "ERRO: rotina com parametros demais (limite de %d)\n",
                    MAX_PARAMS);
            sair_com_erro();
      }

      r->params[r->numParams] = nome;
      r->tiposParams[r->numParams] = tipo;

      r->numParams++;
}

/*
 * buscar_rotina
 * ------------------------------------------------------------
 * Procura uma rotina pelo indice do nome (o lexico da sempre o mesmo
 * indice para o mesmo lexema).
 *
 * Parametro: nome - indice do nome na tabela_simbolos
 * Retorno:   ponteiro para a rotina, ou NULL se o identificador nao for
 *            um procedimento nem uma funcao declarados.
 */
Rotina *buscar_rotina(int nome)
{
      for (int i = 0; i < total_rotinas; i++)
      {
            if (tabela_rotinas[i].nome == nome)
                  return &tabela_rotinas[i];
      }
      return NULL;
}

/*
 * nome_do_token
 * ------------------------------------------------------------
 * Traduz um valor de TokenNome para o texto da listagem e dos erros.
 *
 * Parametro: tipo - valor do enum
 * Retorno:   nome do token, ou "DESCONHECIDO" fora da faixa (protecao).
 */
const char *nome_do_token(TokenNome tipo)
{
      if ((int)tipo < 0 || tipo >= TOKEN_TOTAL || nomes_tokens[tipo] == NULL)
            return "DESCONHECIDO";
      return nomes_tokens[tipo];
}

/*
 * nome_do_op_rel
 * ------------------------------------------------------------
 * Mesma ideia de nome_do_token, mas para o sub-codigo guardado no
 * atributo dos operadores relacionais.
 *
 * Parametro: op - valor do enum OpRelType
 * Retorno:   nome do operador, ou "DESCONHECIDO" fora da faixa.
 */
const char *nome_do_op_rel(OpRelType op)
{
      if ((int)op < 0 || op >= OP_TOTAL || nomes_op_rel[op] == NULL)
            return "DESCONHECIDO";
      return nomes_op_rel[op];
}

/*
 * instrucao_do_op_rel
 * ------------------------------------------------------------
 * Traduz um operador relacional para a instrucao MEPA de comparacao
 * (OP_LT -> CMME, OP_GE -> CMAG, ...).
 *
 * Parametro: op - valor do enum OpRelType
 * Retorno:   nome da instrucao, ou "DESCONHECIDO" fora da faixa.
 */
const char *instrucao_do_op_rel(OpRelType op)
{
      if ((int)op < 0 || op >= OP_TOTAL || instrucoes_op_rel[op] == NULL)
            return "DESCONHECIDO";
      return instrucoes_op_rel[op];
}

/*
 * info_token
 * ------------------------------------------------------------
 * Monta em 'destino' a linha de saida de um token, no formato pedido
 * pelo enunciado:
 *
 *    linha# NOME | atributo
 *
 * O atributo e o indice na tabela de simbolos (ID e STRING), o valor
 * convertido (NUM_INT e NUM_FLOAT), o operador (OP_REL) ou '-'. O buffer
 * vem do chamador, e nao de um 'static' interno, para que duas chamadas
 * no mesmo printf nao sobrescrevam uma a outra.
 *
 * Parametros: token   - token a formatar
 *             destino - buffer de saida
 *             tamanho - capacidade de 'destino', em bytes
 * Retorno:    o proprio 'destino', para permitir uso direto dentro de
 *             um printf.
 */
char *info_token(Token token, char *destino, size_t tamanho)
{
      if (destino == NULL || tamanho == 0)
            return destino;

      switch (token.type)
      {
            case TOKEN_ID:
            case TOKEN_STRING:
                  snprintf(destino, tamanho, "%d# %s | %d",
                           token.line, nome_do_token(token.type),
                           token.attribute.table_index);
                  break;
            case TOKEN_NUM_INT:
                  snprintf(destino, tamanho, "%d# %s | %d",
                           token.line, nome_do_token(token.type),
                           token.attribute.int_value);
                  break;
            case TOKEN_NUM_FLOAT:
                  snprintf(destino, tamanho, "%d# %s | %g",
                           token.line, nome_do_token(token.type),
                           token.attribute.float_value);
                  break;
            case TOKEN_OP_REL:
                  snprintf(destino, tamanho, "%d# %s | %s",
                           token.line, nome_do_token(token.type),
                           nome_do_op_rel(token.attribute.op_code));
                  break;
            default:
                  snprintf(destino, tamanho, "%d# %s | -",
                           token.line, nome_do_token(token.type));
                  break;
      }

      return destino;
}

/*
 * imprimir_token
 * ------------------------------------------------------------
 * Escreve a linha do token na tela e no arquivo de saida (Etapa 2).
 *
 * Parametro: token - token a registrar
 */
void imprimir_token(Token token)
{
      char linha_saida[MAX_LINHA_SAIDA];
      info_token(token, linha_saida, sizeof(linha_saida));
      printf("%s\n", linha_saida);
      fprintf(arquivo_saida, "%s\n", linha_saida);
}

/*
 * igual_sem_caixa
 * ------------------------------------------------------------
 * Compara dois lexemas ignorando maiusculas e minusculas. Escrita a mao
 * porque strcasecmp e POSIX e o MinGW, usado na correcao, nao a garante.
 *
 * Parametros: a, b - cadeias terminadas em '\0'
 * Retorno:    1 quando sao iguais, 0 caso contrario.
 */
int igual_sem_caixa(const char *a, const char *b)
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
 *  O acesso a uma variavel e feito em dois passos, como num compilador
 *  real: o NOME e resolvido para um ENDERECO (busca_tabela_simbolos, que
 *  tambem confere se foi declarada) e dali em diante so o endereco
 *  circula (acessa_var_na_tabela_simbolos). Isso tambem respeita o
 *  enunciado, em que busca_tabela_simbolos devolve o endereco (int).
 *
 *    inserir_var_semant .............. declaracao: cria o no no fim da lista
 *    busca_tabela_simbolos ........... uso: nome -> endereco (ou erro)
 *    acessa_var_na_tabela_simbolos ... endereco -> no (tipo, nome...)
 *    liberar_tabela_simbolos ......... libera os nos no fim da execucao
 * ------------------------------------------------------------ */

/*
 * nome_curto_do_tipo
 * ------------------------------------------------------------
 * Traduz o tipo basico para o nome curto guardado no campo tipo do TNo
 * (o campo tem so 7 posicoes).
 *
 * Parametro: tipo - TOKEN_INTEIRO, TOKEN_REAL, TOKEN_CARACTERE ou
 *                   TOKEN_LOGICO
 * Retorno:   "int", "real", "char" ou "logico" ("?" como protecao).
 */
const char *nome_curto_do_tipo(TokenNome tipo)
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
 * inserir_var_semant
 * ------------------------------------------------------------
 * Registra uma variavel no FIM da minitabela, com o proximo endereco.
 *
 * Como inserir no fim ja percorre a lista inteira, o mesmo laco confere
 * se o nome ja foi declarado (regra 3: identificador unico). A
 * comparacao diferencia maiusculas, como o inserir_simbolo do lexico.
 *
 * Parametros: nome  - nome da variavel
 *             tipo  - tipo basico declarado (TOKEN_INTEIRO, ...)
 *             linha - linha da declaracao, para as mensagens de erro
 */
void inserir_var_semant(const char *nome, TokenNome tipo, int linha)
{
      /* o campo ID tem 16 posicoes; truncar poderia igualar dois nomes */
      if (strlen(nome) > 15)
            erro_semantico(linha, "nome de variavel com mais de 15 caracteres");

      TNo *ultimo = NULL;
      for (TNo *atual = minitabela; atual != NULL; atual = atual->prox)
      {
            if (strcmp(atual->ID, nome) == 0)
                  erro_semantico(linha, "variavel declarada mais de uma vez");
            ultimo = atual;
      }

      TNo *novo = malloc(sizeof(TNo));
      if (novo == NULL)
      {
            printf("ERRO: memoria insuficiente para a tabela de simbolos\n");
            fprintf(arquivo_saida,
                    "ERRO: memoria insuficiente para a tabela de simbolos\n");
            sair_com_erro();
      }

      /* os tamanhos ja foram garantidos, entao strcpy nao estoura */
      strcpy(novo->ID, nome);
      strcpy(novo->tipo, nome_curto_do_tipo(tipo));
      novo->endereco = prox_endereco++;
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
 * Funcao pedida pelo enunciado: recebe o nome do identificador e devolve
 * o endereco da variavel na minitabela. Se o nome nao foi declarado, e
 * erro semantico (regra 1), entao quem chama nao precisa testar o
 * retorno. O parametro linha foi acrescentado para a mensagem de erro.
 *
 * Parametros: atributo_ID - nome da variavel (ver nome_do_simbolo)
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

      erro_semantico(linha, "variavel nao declarada");
      return -1;  /* nunca chega aqui: erro_semantico encerra o programa */
}

/*
 * existe_na_minitabela
 * ------------------------------------------------------------
 * Diz se ha uma variavel com esse nome, sem gerar erro (ao contrario de
 * busca_tabela_simbolos). Usada para impedir que uma rotina tenha o nome
 * de uma variavel (regra 3).
 *
 * Parametro: nome - nome a procurar
 * Retorno:   1 se existe, 0 se nao.
 */
int existe_na_minitabela(const char *nome)
{
      for (TNo *atual = minitabela; atual != NULL; atual = atual->prox)
      {
            if (strcmp(atual->ID, nome) == 0)
                  return 1;
      }
      return 0;
}

/*
 * acessa_var_na_tabela_simbolos
 * ------------------------------------------------------------
 * Devolve o no da variavel a partir do endereco de busca_tabela_simbolos,
 * para consultar o tipo.
 *
 * Nao compara nomes: como os nos sao inseridos sempre no fim e nunca
 * removidos, o no de endereco k e o k-esimo da lista. O acesso por
 * endereco e "simulado", porque a lista continua sendo percorrida; num
 * compilador maior a tabela seria um vetor ou um hashmap, com acesso
 * direto.
 *
 * Parametro: endereco - endereco devolvido por busca_tabela_simbolos
 * Retorno:   o no, ou NULL se o endereco nao existir (nao deve acontecer
 *            com um endereco vindo de busca_tabela_simbolos).
 */
TNo *acessa_var_na_tabela_simbolos(int endereco)
{
      TNo *atual = minitabela;
      for (int i = 0; i < endereco && atual != NULL; i++)
            atual = atual->prox;
      return atual;
}

/*
 * liberar_tabela_simbolos
 * ------------------------------------------------------------
 * Libera os nos da minitabela (no fim do main e em sair_com_erro).
 */
void liberar_tabela_simbolos(void)
{
      TNo *atual = minitabela;
      while (atual != NULL)
      {
            /* guarda o proximo antes do free */
            TNo *proximo = atual->prox;
            free(atual);
            atual = proximo;
      }
      minitabela = NULL;
}

/* ------------------------------------------------------------
 *  Fase 2 - tipos das expressoes (regra 5)
 * ------------------------------------------------------------
 *  As expressoes devolvem o tipo como TokenNome, como os parametros e o
 *  retorno das rotinas. Regra de mistura combinada pelo grupo:
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
 * tipo_da_variavel
 * ------------------------------------------------------------
 * Caminho inverso de nome_curto_do_tipo: le o tipo guardado na
 * minitabela e devolve o TokenNome, para comparar com os outros tipos.

 *
 * Parametro: endereco - endereco devolvido por busca_tabela_simbolos
 * Retorno:   TOKEN_INTEIRO, TOKEN_REAL, TOKEN_CARACTERE ou TOKEN_LOGICO.
 */
TokenNome tipo_da_variavel(int endereco)
{
      const char *tipo = acessa_var_na_tabela_simbolos(endereco)->tipo;

      if (strcmp(tipo, nome_curto_do_tipo(TOKEN_INTEIRO)) == 0)
            return TOKEN_INTEIRO;
      if (strcmp(tipo, nome_curto_do_tipo(TOKEN_REAL)) == 0)
            return TOKEN_REAL;
      if (strcmp(tipo, nome_curto_do_tipo(TOKEN_CARACTERE)) == 0)
            return TOKEN_CARACTERE;
      return TOKEN_LOGICO;
}

/*
 * eh_numerico
 * ------------------------------------------------------------
 * Retorno: 1 se o tipo e inteiro ou real, 0 caso contrario.
 */
int eh_numerico(TokenNome tipo)
{
      return tipo == TOKEN_INTEIRO || tipo == TOKEN_REAL;
}

/*
 * tipo_aritmetico
 * ------------------------------------------------------------
 * Confere os dois lados de "+", "-" ou "*" e calcula o tipo do
 * resultado: real se algum lado for real, inteiro se os dois forem.
 *
 * Parametros: a, b  - tipos dos operandos
 *             linha - linha do operador, para a mensagem de erro
 * Retorno:    TOKEN_INTEIRO ou TOKEN_REAL.
 */
TokenNome tipo_aritmetico(TokenNome a, TokenNome b, int linha)
{
      if (!eh_numerico(a) || !eh_numerico(b))
            erro_semantico(linha, "operador aritmetico exige operandos numericos");

      if (a == TOKEN_REAL || b == TOKEN_REAL)
            return TOKEN_REAL;
      return TOKEN_INTEIRO;
}

/*
 * tipo_atribuivel
 * ------------------------------------------------------------
 * Diz se um valor do tipo origem pode ser guardado no tipo destino:
 * mesmo tipo, ou inteiro em real. Real em inteiro nao pode (perderia a
 * parte decimal). Vale para a atribuicao, os argumentos (regra 6) e o
 * retorne (regra 9).
 *
 * Parametros: destino - tipo da variavel, parametro ou funcao
 *             origem  - tipo da expressao
 * Retorno:    1 se pode, 0 se nao.
 */
int tipo_atribuivel(TokenNome destino, TokenNome origem)
{
      if (destino == origem)
            return 1;
      return destino == TOKEN_REAL && origem == TOKEN_INTEIRO;
}

/* ------------------------------------------------------------
 *  Fase 2 - palavra reservada como nome de variavel (regra 2)
 * ------------------------------------------------------------
 *  O lexico transforma "se" em TOKEN_SE, entao "se: inteiro" nunca chega
 *  como TOKEN_ID e viraria um erro sintatico. Como o enunciado trata a
 *  regra como semantica, os pontos onde se espera o nome de uma variavel
 *  (registrar_id, parametro e secao_var) reconhecem a palavra reservada
 *  e dao erro semantico.
 * ------------------------------------------------------------ */

/* eh_palavra_reservada depende de as palavras reservadas estarem em
 * sequencia no enum (TOKEN_ALGORITMO ate TOKEN_MOD); se a ordem mudar,
 * a compilacao quebra aqui. */
_Static_assert(TOKEN_PONTO_PONTO + 1 == TOKEN_ALGORITMO,
               "TOKEN_ALGORITMO deve ser a primeira palavra reservada do enum");
_Static_assert(TOKEN_MOD + 1 == TOKEN_TOTAL,
               "TOKEN_MOD deve ser a ultima palavra reservada do enum");

/*
 * eh_palavra_reservada
 * ------------------------------------------------------------
 * Diz se um token e uma palavra reservada da linguagem.
 *
 * Parametro: t - tipo do token
 * Retorno:   1 se e palavra reservada, 0 caso contrario.
 */
int eh_palavra_reservada(TokenNome t)
{
      return t >= TOKEN_ALGORITMO && t <= TOKEN_MOD;
}

/*
 * recusar_palavra_reservada
 * ------------------------------------------------------------
 * Chamada onde o parser espera o nome de uma variavel ou parametro:
 * se o token e uma palavra reservada, e erro semantico (regra 2). Os
 * outros tokens continuam sendo tratados pelo consumir(TOKEN_ID).
 */
void recusar_palavra_reservada(void)
{
      if (eh_palavra_reservada(lookahead.type))
            erro_semantico(lookahead.line,
                          "palavra reservada nao pode ser usada como nome de variavel");
}

/* ============================================================
 *  4. TRATAMENTO DE ERROS
 * ============================================================
 *  Todas as funcoes de erro mostram a mensagem com a linha do fonte e
 *  encerram o processo por sair_com_erro, que fecha os arquivos (senao
 *  os tokens ja escritos ficariam no buffer e o arquivo sairia vazio) e
 *  apaga o saida_mepa.txt incompleto.
 * ============================================================ */

/*
 * sair_com_erro
 * ------------------------------------------------------------
 * Ponto unico de saida em caso de erro: fecha os arquivos, apaga o
 * saida_mepa.txt (um MEPA pela metade pareceria valido), libera a
 * memoria e termina com codigo 1. Os testes de NULL cobrem erros que
 * acontecem antes de os arquivos estarem abertos.
 */
void sair_com_erro(void)
{
      if (arquivo_saida != NULL)
            fclose(arquivo_saida);

      if (arquivo_mepa != NULL)
      {
            fclose(arquivo_mepa);
            remove(ARQUIVO_MEPA);
      }

      free(buffer);
      liberar_tabela_simbolos();
      exit(1);
}

/*
 * erro_sintatico
 * ------------------------------------------------------------
 * Relata um token que nao era o esperado pela gramatica e encerra.
 *
 * Parametros: token    - token encontrado (traz a linha do fonte)
 *             esperado - texto do que a gramatica esperava ali
 */
void erro_sintatico(Token token, const char *esperado)
{
      printf("%d# ERRO SINTATICO: esperado %s, encontrado %s\n",
             token.line, esperado, nome_do_token(token.type));
      fprintf(arquivo_saida, "%d# ERRO SINTATICO: esperado %s, encontrado %s\n",
              token.line, esperado, nome_do_token(token.type));
      sair_com_erro();
}

/*
 * erro_lexico
 * ------------------------------------------------------------
 * Relata uma sequencia que nao forma nenhum lexema valido e encerra.
 *
 * Parametros: linha     - linha do fonte onde o erro foi localizado
 *             sequencia - trecho lexicamente errado
 */
void erro_lexico(int linha, const char *sequencia)
{
      printf("%d# ERRO LEXICO: '%s'\n", linha, sequencia);
      fprintf(arquivo_saida, "%d# ERRO LEXICO: '%s'\n", linha, sequencia);
      sair_com_erro();
}

/*
 * erro_semantico
 * ------------------------------------------------------------
 * Relata uma construcao sintaticamente correta mas sem sentido para a
 * linguagem (ex.: procedimento usado em expressao) e encerra.
 *
 * Parametros: linha    - linha do fonte onde o erro foi localizado
 *             mensagem - descricao do erro
 */
void erro_semantico(int linha, const char *mensagem)
{
      printf("%d# ERRO SEMANTICO: %s\n", linha, mensagem);
      fprintf(arquivo_saida, "%d# ERRO SEMANTICO: %s\n", linha, mensagem);
      sair_com_erro();
}

/*
 * nao_suportado
 * ------------------------------------------------------------
 * Encerra quando o programa usa um recurso que a geracao de codigo nao
 * implementa (vetores, mod e textos fora do escreva; ver docs/etapa2.md,
 * secao 4). O programa esta correto, a limitacao e do compilador; por
 * isso a mensagem e diferente da do erro semantico.
 *
 * Parametros: linha   - linha do fonte onde o recurso aparece
 *             recurso - descricao do recurso, como "vetores"
 */
void nao_suportado(int linha, const char *recurso)
{
      printf("%d# NAO SUPORTADO: geracao de codigo para %s\n", linha, recurso);
      fprintf(arquivo_saida, "%d# NAO SUPORTADO: geracao de codigo para %s\n",
              linha, recurso);
      sair_com_erro();
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
 *  O fim de cada lexema e decidido pelo proximo caractere, e nao pelo
 *  espaco, porque alguns exemplos do anexo nao tem espaco
 *  (escreval("...")).
 * ============================================================ */

/*
 * pular_espacos_e_comentarios
 * ------------------------------------------------------------
 * Avanca sobre espacos, tabulacoes, quebras de linha, o '\r' do Windows
 * e comentarios "//". E aqui que linha_atual e incrementada.
 */
void pular_espacos_e_comentarios(void)
{
      while (buffer[posicao] != '\0')
      {
            char c = buffer[posicao];

            if (c == '\n')
            {
                  linha_atual++;
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
 * reconhecer_token
 * ------------------------------------------------------------
 * Le o proximo lexema e devolve o token, ja com o atributo: e aqui que
 * as expressoes regulares sao implementadas. Ordem dos testes: fim de
 * arquivo, numero, cadeia, identificador ou palavra reservada e
 * simbolos; o resto e erro lexico.
 *
 * E 'static' porque so obter_token deve chama-la (ela tambem registra o
 * token na listagem).
 *
 * Retorno: o proximo token do fonte (TOKEN_EOF quando o fonte acaba).
 */
static Token reconhecer_token(void)
{
      Token token;

      pular_espacos_e_comentarios();

      /* a linha e fixada antes de consumir o lexema: e a linha em que
       * ele comeca */
      token.line = linha_atual;
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
            int eh_real = 0;
            int inicio = posicao;

            while (isdigit((unsigned char)buffer[posicao]))
                  posicao++;

            if (buffer[posicao] == '.')
            {
                  if (isdigit((unsigned char)buffer[posicao + 1]))
                  {
                        /* 1.60 -> consome o ponto e a parte decimal */
                        eh_real = 1;
                        posicao++;
                        while (isdigit((unsigned char)buffer[posicao]))
                              posicao++;
                  }
                  else if (buffer[posicao + 1] != '.')
                  {
                        /* "1." sem digito depois e um real malformado;
                         * em "1..4" o numero termina aqui e o ".." vira
                         * o proximo token */
                        int parcial = posicao - inicio + 1;
                        if (parcial >= MAX_LEXEMA)
                              parcial = MAX_LEXEMA - 1;
                        memcpy(lexema, buffer + inicio, parcial);
                        lexema[parcial] = '\0';
                        erro_lexico(token.line, lexema);
                  }
            }

            int tamanho = posicao - inicio;

            if (tamanho >= MAX_LEXEMA)
            {
                  memcpy(lexema, buffer + inicio, MAX_LEXEMA - 1);
                  lexema[MAX_LEXEMA - 1] = '\0';
                  erro_lexico(token.line, lexema);
            }
            memcpy(lexema, buffer + inicio, tamanho);
            lexema[tamanho] = '\0';

            if (eh_real)
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

            /* uma cadeia nao atravessa linhas: sem fechar, e erro na
             * linha em que comecou */
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
                  erro_lexico(token.line, lexema);
            }

            posicao++;   /* consome a aspa de fechamento */

            int tamanho = posicao - inicio;

            if (tamanho >= MAX_LEXEMA)
            {
                  memcpy(lexema, buffer + inicio, MAX_LEXEMA - 1);
                  lexema[MAX_LEXEMA - 1] = '\0';
                  erro_lexico(token.line, lexema);
            }
            memcpy(lexema, buffer + inicio, tamanho);
            lexema[tamanho] = '\0';

            /* guardado com as aspas, para que a cadeia "idade" e a
             * variavel idade nao ocupem o mesmo indice */
            token.type = TOKEN_STRING;
            token.attribute.table_index = inserir_simbolo(lexema);

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

            /* lexema longo demais e erro: truncar poderia igualar dois
             * identificadores diferentes */
            if (tamanho >= MAX_LEXEMA)
            {
                  memcpy(lexema, buffer + inicio, MAX_LEXEMA - 1);
                  lexema[MAX_LEXEMA - 1] = '\0';
                  erro_lexico(token.line, lexema);
            }
            memcpy(lexema, buffer + inicio, tamanho);
            lexema[tamanho] = '\0';

            /* assume identificador e rebaixa para palavra reservada se
             * o lexema estiver na tabela */
            token.type = TOKEN_ID;
            int n = sizeof(palavras_reservadas) / sizeof(palavras_reservadas[0]);
            for (int i = 0; i < n; i++)
            {
                  if (igual_sem_caixa(lexema, palavras_reservadas[i].lexema))
                  {
                        token.type = palavras_reservadas[i].token;
                        break;
                  }
            }

            /* so identificador ocupa lugar na tabela de simbolos */
            if (token.type == TOKEN_ID)
                  token.attribute.table_index = inserir_simbolo(lexema);

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
                  /* "//" ja foi consumido como comentario: e divisao */
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
                  /* '<' pode abrir "<=", "<>" ou "<-", ou ser so o "<" */
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
                  /* so existe ".." (faixa de vetor); "." sozinho e erro */
                  posicao++;                    /* consome o 1o ponto */
                  if (buffer[posicao] == '.')
                  {
                        token.type = TOKEN_PONTO_PONTO;
                        posicao++;              /* consome o 2o ponto */
                  }
                  else
                  {
                        char seq[2] = { '.', '\0' };
                        erro_lexico(token.line, seq);
                  }
                  break;
            default:
            {
                  /* caractere que nao pertence ao alfabeto da linguagem */
                  char seq[2] = { buffer[posicao], '\0' };
                  erro_lexico(token.line, seq);
            }
      }

      return token;
}

/*
 * obter_token
 * ------------------------------------------------------------
 * Interface do lexico para o parser (Figura 1 do enunciado).
 *
 * Pede o proximo token e ja o registra na listagem, que sai assim na
 * mesma passagem da analise. E seguro porque a gramatica e LL(1): o
 * parser nunca volta atras, entao nenhum token e impresso duas vezes.
 *
 * Retorno: o proximo token do fonte.
 */
Token obter_token(void)
{
      Token token = reconhecer_token();
      imprimir_token(token);
      return token;
}

/* ============================================================
 *  6. GERACAO DE CODIGO MEPA  -  Fase 2
 * ============================================================
 *  A geracao e dirigida pela sintaxe: o parser (secao 7) chama estas
 *  funcoes ao reconhecer cada construcao. Elas sabem COMO escrever uma
 *  linha; o parser decide QUANDO e O QUE. Uma funcao por formato:
 *
 *    imprimir ................ sem operando ......... SOMA, INPP, IMPR
 *    imprimir_nome ........... nome de variavel ..... CRVL idade
 *    imprimir_inteiro ........ inteiro .............. CRCT 4, AMEM 3
 *    imprimir_dois_inteiros .. dois inteiros ........ RTPR 1,0
 *    imprimir_real ........... real ................. CRCT 1.6
 *    imprimir_desvio ......... rotulo usado ......... DSVF L3, CHPR L2
 *    imprimir_rotulo ......... rotulo definido ...... L3: NADA
 *
 *  Instrucoes recuadas com 4 espacos, rotulos na coluna 0 e variaveis
 *  referenciadas pelo nome, como pede o enunciado.

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
      fprintf(arquivo_mepa, "    %s\n", instr);
}

/*
 * imprimir_nome
 * ------------------------------------------------------------
 * Escreve uma instrucao cujo operando e o nome de uma variavel.
 * Exemplo: "    CRVL idade"
 *
 * Parametros: instr - mnemonico da instrucao
 *             nome  - nome da variavel (ver nome_do_simbolo)
 */
void imprimir_nome(const char *instr, const char *nome)
{
      fprintf(arquivo_mepa, "    %s %s\n", instr, nome);
}

/*
 * imprimir_inteiro
 * ------------------------------------------------------------
 * Escreve uma instrucao com operando inteiro.  Exemplo: "    CRCT 4"
 *
 * Parametros: instr - mnemonico da instrucao
 *             valor - operando
 */
void imprimir_inteiro(const char *instr, int valor)
{
      fprintf(arquivo_mepa, "    %s %d\n", instr, valor);
}

/*
 * imprimir_dois_inteiros
 * ------------------------------------------------------------
 * Escreve uma instrucao com dois operandos inteiros (formato do RTPR
 * k,n).  Exemplo: "    RTPR 1,0"
 *
 * Parametros: instr  - mnemonico da instrucao
 *             valor1 - primeiro operando
 *             valor2 - segundo operando
 */
void imprimir_dois_inteiros(const char *instr, int valor1, int valor2)
{
      fprintf(arquivo_mepa, "    %s %d,%d\n", instr, valor1, valor2);
}

/*
 * imprimir_real
 * ------------------------------------------------------------
 * Escreve uma instrucao com operando real. O %g evita zeros inuteis
 * (1.6 em vez de 1.600000).  Exemplo: "    CRCT 1.6"
 *
 * Parametros: instr - mnemonico da instrucao
 *             valor - operando
 */
void imprimir_real(const char *instr, double valor)
{
      fprintf(arquivo_mepa, "    %s %g\n", instr, valor);
}

/*
 * imprimir_desvio
 * ------------------------------------------------------------
 * Escreve uma instrucao que USA um rotulo (desvios e chamada de
 * rotina).  Exemplo: "    DSVF L3"
 *
 * Parametros: instr  - DSVS, DSVF ou CHPR
 *             rotulo - numero do rotulo (o "L" e acrescentado aqui)
 */
void imprimir_desvio(const char *instr, int rotulo)
{
      fprintf(arquivo_mepa, "    %s L%d\n", instr, rotulo);
}

/*
 * imprimir_rotulo
 * ------------------------------------------------------------
 * DEFINE um rotulo, para onde os desvios pulam. O NADA garante uma
 * instrucao no rotulo, mesmo no fim de um bloco.  Exemplo: "L3: NADA"
 *
 * Parametro: rotulo - numero do rotulo
 */
void imprimir_rotulo(int rotulo)
{
      fprintf(arquivo_mepa, "L%d: NADA\n", rotulo);
}

/*
 * proximo_rotulo
 * ------------------------------------------------------------
 * Funcao sugerida pelo enunciado: devolve o proximo rotulo (1, 2, 3...).
 *
 * Por causa da recursao do parser, cada comando pede os seus rotulos no
 * inicio e os guarda em variaveis LOCAIS: um se dentro de outro pede os
 * proprios rotulos sem sobrescrever os do externo.
 *
 * Retorno: numero do novo rotulo.
 */
int proximo_rotulo(void)
{
      static int contador_rotulos = 0;
      return ++contador_rotulos;
}

/*
 * nome_do_simbolo
 * ------------------------------------------------------------
 * Devolve o lexema de um identificador a partir do table_index do
 * token, para ser usado como operando no MEPA.
 *
 * Parametro: indice - indice na tabela_simbolos
 * Retorno:   o nome (const: quem recebe nao pode alterar a tabela).
 */
const char *nome_do_simbolo(int indice)
{
      return tabela_simbolos[indice];
}

/*
 * gerar_chamada
 * ------------------------------------------------------------
 * Gera a parte comum a toda chamada de rotina, depois que o codigo dos
 * argumentos ja foi emitido (eles estao empilhados, o ultimo no topo):
 *
 *    ARMZ ultimo_parametro      <- o ARMZ retira o topo, por isso
 *    ...                           os parametros sao percorridos do
 *    ARMZ primeiro_parametro       ultimo para o primeiro
 *    CHPR Lrotina
 *
 * Os parametros sao posicoes de memoria com nome (docs/etapa2.md, secao
 * 3). Numa funcao, quem chama emite depois o CRVL com o nome dela.
 *
 * Verificacoes semanticas (regra 6), antes de emitir a chamada:
 *   numero - a quantidade de argumentos tem de ser a de parametros
 *            (senao os ARMZ sairiam descasados). E conferido primeiro:
 *            depois dele, todas as posicoes de tipos estao preenchidas.
 *   tipo   - cada argumento tem de caber no parametro, pela regra da
 *            atribuicao (tipo_atribuivel).
 *
 * Parametros: r        - rotina chamada
 *             num_args - quantidade de argumentos da chamada
 *             linha    - linha da chamada, para as mensagens de erro
 *             tipos    - tipo de cada argumento, na ordem; pode ser NULL
 *                        quando num_args e 0 (o vetor nunca e lido)
 */
void gerar_chamada(Rotina *r, int num_args, int linha, TokenNome tipos[])
{
      if (num_args != r->numParams)
            erro_semantico(linha,
                          "quantidade de argumentos incompativel com a declaracao da rotina");

      /* do primeiro para o ultimo, para apontar o primeiro argumento
       * errado. Destino e o parametro, origem e o argumento */
      for (int i = 0; i < r->numParams; i++)
      {
            if (!tipo_atribuivel(r->tiposParams[i], tipos[i]))
            {
                  char mensagem[80];
                  /* i + 1: para quem le o erro, o primeiro argumento e o 1 */
                  snprintf(mensagem, sizeof mensagem,
                           "tipo do argumento %d incompativel com o parametro",
                           i + 1);
                  erro_semantico(linha, mensagem);
            }
      }

      /* do ultimo para o primeiro: o ARMZ retira o topo da pilha */
      for (int i = r->numParams - 1; i >= 0; i--)
            imprimir_nome("ARMZ", nome_do_simbolo(r->params[i]));

      imprimir_desvio("CHPR", r->rotulo);
}

/*
 * recusar_texto_sem_codigo
 * ------------------------------------------------------------
 * Um texto ("abc") nao gera codigo, porque a MEPA nao tem instrucao para
 * texto. No escreva isso basta, mas numa atribuicao, argumento, retorne
 * ou comparacao o valor faltaria na pilha (o ARMZ de nome <- "abc"
 * encontraria a pilha vazia).
 *
 * Quem gera codigo com o valor de uma expressao guarda textos_sem_codigo
 * antes dela e chama esta funcao depois: se o contador mudou, havia um
 * texto, e o recurso e tratado como nao suportado. A chamada vem DEPOIS
 * da conferencia de tipos, para que x <- "texto" com x inteiro continue
 * sendo erro de tipo.
 *
 * Parametros: textos_antes - valor de textos_sem_codigo antes da expressao
 *             linha        - linha da construcao, para a mensagem
 */
void recusar_texto_sem_codigo(int textos_antes, int linha)
{
      if (textos_sem_codigo != textos_antes)
            nao_suportado(linha, "textos fora do escreva");
}

/* ============================================================
 *  7. ANALISADOR SINTATICO (PARSER)  -  Etapa 3
 * ============================================================
 *  Analise descendente recursiva: uma funcao por nao-terminal, com a
 *  producao escolhida pelo token atual (lookahead). As mesmas funcoes
 *  fazem as verificacoes semanticas e emitem o codigo MEPA (secao 6).
 *
 *  Em quase todas, o atributo de um token (nome, valor, operador) e
 *  guardado ANTES de consumi-lo, porque depois o lookahead ja e o token
 *  seguinte. A gramatica implementada e:
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
 *  A cadeia expr -> expr_e -> expr_rel -> expr_arit -> termo -> fator da
 *  a precedencia (OU e o mais fraco) e produz o codigo em ordem pos-fixa:
 *  o operador so e emitido depois dos dois operandos.
 * ============================================================ */

/*
 * next_token
 * ------------------------------------------------------------
 * Pede o proximo token ao lexico e guarda em lookahead.
 */
void next_token(void)
{
      lookahead = obter_token();
}

/*
 * consumir
 * ------------------------------------------------------------
 * Confere se o token atual e o esperado e avanca; senao, erro sintatico.
 *
 * Parametro: esperado - token exigido pela gramatica neste ponto
 */
void consumir(TokenNome esperado)
{
      if (lookahead.type == esperado)
            next_token();
      else
            erro_sintatico(lookahead, nome_do_token(esperado));
}

/* Prototipos: as producoes se chamam em ciclo (expr chega em fator, que
 * volta em expr). */
void conferir_variaveis_nao_usadas(void);
void programa(void);
void declaracoes(void);
void secao_var(void);
int encerra_secao_var(TokenNome t);
void reservada_na_declaracao(void);
void decl_var(void);
int lista_ids(int indices[], int linhas[]);
int tipo(TokenNome *tipo_basico_lido);
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
int argumentos(TokenNome tipos[]);
TokenNome expr(void);
TokenNome expr_e(void);
TokenNome expr_rel(void);
TokenNome expr_arit(void);
TokenNome termo(void);
TokenNome fator(void);

/*
 * argumentos -> expr { "," expr }
 * ------------------------------------------------------------
 * Argumentos de uma chamada de rotina: o codigo de cada um e emitido em
 * ordem, deixando os valores empilhados para gerar_chamada. (O escreva
 * tem laco proprio, para emitir um IMPR depois de cada argumento.)
 *
 * O tipo de cada argumento vai para tipos, para a regra 6. O vetor e
 * declarado por quem chama, dentro do bloco da chamada: em f(g(1), 2),
 * g e chamada no meio dos argumentos de f, e cada uma precisa do seu.
 *
 * Parametro: tipos - recebe o tipo de cada argumento, na ordem; deve ter
 *                    MAX_PARAMS posicoes
 * Retorno:   quantidade de argumentos, conferida com a declaracao da
 *            rotina em gerar_chamada.
 */
int argumentos(TokenNome tipos[])
{
      int contador = 1;
      tipos[0] = expr();
      while (lookahead.type == TOKEN_VIRGULA)
      {
            consumir(TOKEN_VIRGULA);

            /* expr e chamada sempre: e ela que consome o argumento */
            TokenNome tipo = expr();

            /* so grava se couber; com argumentos demais, a contagem
             * segue e gerar_chamada da o erro de quantidade */
            if (contador < MAX_PARAMS)
                  tipos[contador] = tipo;

            contador++;
      }
      return contador;
}

/*
 * expr -> expr_e { OU expr_e }
 * ------------------------------------------------------------
 * Operador logico OU, o de menor precedencia.
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
                  erro_semantico(linha, "operador logico exige operandos logicos");
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
                  erro_semantico(linha, "operador logico exige operandos logicos");
            imprimir("CONJ");
      }
      return tipo;
}

/*
 * expr_rel -> expr_arit [ OP_REL expr_arit ]
 * ------------------------------------------------------------
 * Comparacao. E um 'if', e nao um 'while', porque "a < b < c" nao faz
 * sentido na linguagem.
 *
 * Codigo gerado: <expr_arit> <expr_arit> CMxx (ver instrucoes_op_rel),
 * que deixa 1 ou 0 na pilha.
 *
 * Verificacao semantica: regra 5. < > <= >= comparam so numeros; = e <>
 * aceitam tambem dois valores do mesmo tipo. Texto na comparacao e
 * recurso nao suportado.
 *
 * Retorno: tipo da expressao (o de expr_arit se nao houver comparacao;
 *          logico se houver).
 */
TokenNome expr_rel(void)
{
      int textos_antes = textos_sem_codigo;
      TokenNome tipo = expr_arit();
      if (lookahead.type == TOKEN_OP_REL)
      {
            /* operador e linha guardados antes de consumir o token */
            OpRelType op = lookahead.attribute.op_code;
            int linha = lookahead.line;
            consumir(TOKEN_OP_REL);
            TokenNome direita = expr_arit();

            int numeros = eh_numerico(tipo) && eh_numerico(direita);
            int igualdade = (op == OP_EQ || op == OP_NE);
            if (!numeros && !(igualdade && tipo == direita))
                  erro_semantico(linha, "comparacao entre tipos incompativeis");
            recusar_texto_sem_codigo(textos_antes, linha);

            imprimir(instrucao_do_op_rel(op));
            tipo = TOKEN_LOGICO;
      }
      return tipo;
}

/*
 * expr_arit -> termo { ( "+" | "-" ) termo }
 * ------------------------------------------------------------
 * Soma e subtracao.
 *
 * Codigo gerado: <termo> <termo> SOMA | SUBT. Cada operacao sai assim
 * que o segundo operando termina, o que da a associatividade a esquerda:
 * 10 - 4 - 3 vira (10 - 4) - 3.
 *
 * Verificacao semantica: regra 5 (numeros dos dois lados).
 *
 * Retorno: tipo da expressao (real se algum operando for real).
 */
TokenNome expr_arit(void)
{
      TokenNome tipo = termo();
      while (lookahead.type == TOKEN_SOMA || lookahead.type == TOKEN_SUB)
      {
            /* operador e linha guardados antes de avancar */
            TokenNome op = lookahead.type;
            int linha = lookahead.line;
            next_token();
            TokenNome direita = termo();
            tipo = tipo_aritmetico(tipo, direita, linha);

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
 * Multiplicacao, divisao real, divisao inteira e resto.
 *
 * Codigo gerado: <fator> <fator> MULT | DIVF | DIVI. O mod nao e
 * suportado na geracao de codigo.
 *
 * Verificacao semantica: regra 5. "*" e "/" exigem numeros; "\" e mod
 * exigem inteiros.
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
            next_token();
            TokenNome direita = fator();

            switch (op)
            {
                  case TOKEN_MULT:
                        tipo = tipo_aritmetico(tipo, direita, linha);
                        imprimir("MULT");
                        break;
                  case TOKEN_DIV:
                        /* no Visualg "/" e sempre divisao real */
                        tipo_aritmetico(tipo, direita, linha);
                        tipo = TOKEN_REAL;
                        imprimir("DIVF");
                        break;
                  case TOKEN_DIV_INT:
                  case TOKEN_MOD:
                        if (tipo != TOKEN_INTEIRO || direita != TOKEN_INTEIRO)
                              erro_semantico(linha,
                                             "operadores \\ e mod exigem operandos inteiros");
                        tipo = TOKEN_INTEIRO;

                        if (op == TOKEN_DIV_INT)
                              imprimir("DIVI");
                        else
                              /* a MEPA nao tem instrucao de resto
                               * (ver docs/etapa2.md, secao 4) */
                              nao_suportado(linha, "o operador mod");
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
 * Unidade indivisivel da expressao. Depois de um ID, o proximo token diz
 * se e variavel simples, elemento de vetor ou chamada de funcao.
 *
 * Codigo gerado:
 *    numero ............ CRCT valor
 *    verdadeiro/falso .. CRCT 1 / CRCT 0
 *    variavel .......... CRVL nome
 *    "-" fator ......... <fator> INVR
 *    funcao(args) ...... <args> ARMZ params CHPR Lf CRVL funcao
 *    "texto" ........... nada (tratado pelo escreva)
 *
 * Verificacoes semanticas: regra 1 (variavel declarada), regra 5 (menos
 * unario so em numero), regra 7 (procedimento em expressao) e regra 6,
 * em gerar_chamada.
 *
 * Retorno: tipo do fator (do literal, da variavel ou o de retorno da
 *          funcao). Devolvido num unico return no fim, porque o
 *          compilador nao sabe que erro_sintatico nunca retorna.
 */
TokenNome fator(void)
{
      TokenNome tipo = TOKEN_INTEIRO;
      switch (lookahead.type)
      {
            case TOKEN_NUM_INT:
                  /* o valor e lido antes de avancar para o proximo token */
                  imprimir_inteiro("CRCT", lookahead.attribute.int_value);
                  next_token();
                  tipo = TOKEN_INTEIRO;
                  break;
            case TOKEN_NUM_FLOAT:
                  imprimir_real("CRCT", lookahead.attribute.float_value);
                  next_token();
                  tipo = TOKEN_REAL;
                  break;
            case TOKEN_STRING:
                  /* texto nao gera codigo; o contador avisa quem usar o
                   * valor (ver recusar_texto_sem_codigo) */
                  next_token();
                  textos_sem_codigo++;
                  tipo = TOKEN_CARACTERE;
                  break;
            case TOKEN_VERDADEIRO:
                  imprimir_inteiro("CRCT", 1);
                  next_token();
                  tipo = TOKEN_LOGICO;
                  break;
            case TOKEN_FALSO:
                  imprimir_inteiro("CRCT", 0);
                  next_token();
                  tipo = TOKEN_LOGICO;
                  break;
            case TOKEN_SUB:
            {
                  /* menos unario: primeiro o operando, depois o INVR */
                  int linha = lookahead.line;
                  consumir(TOKEN_SUB);
                  tipo = fator();
                  if (!eh_numerico(tipo))
                        erro_semantico(linha, "menos unario exige operando numerico");
                  imprimir("INVR");
                  break;
            }
            case TOKEN_ABRE_PAR:
                  /* parenteses nao geram instrucao */
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
                        nao_suportado(linha, "vetores");
                        consumir(TOKEN_ABRE_COL);
                        expr();
                        consumir(TOKEN_FECHA_COL);
                  }
                  else if (lookahead.type == TOKEN_ABRE_PAR)
                  {
                        /* chamada de funcao: eh_par(num) */
                        Rotina *r = buscar_rotina(idx);
                        if (r == NULL)
                              erro_semantico(linha, "identificador nao e uma funcao");
                        if (!r->ehFuncao)
                              erro_semantico(linha, "procedimento nao pode ser usado em expressao");

                        consumir(TOKEN_ABRE_PAR);

                        TokenNome tipos[MAX_PARAMS];
                        int textos_antes = textos_sem_codigo;
                        int num_args = argumentos(tipos);
                        consumir(TOKEN_FECHA_PAR);
                        gerar_chamada(r, num_args, linha, tipos);
                        recusar_texto_sem_codigo(textos_antes, linha);

                        /* o retorno fica na posicao com o nome da funcao */
                        imprimir_nome("CRVL", nome_do_simbolo(r->nome));
                        tipo = r->tipoRetorno;
                  }
                  else
                  {
                        /* variavel simples: tem de ter sido declarada */
                        int endereco = busca_tabela_simbolos(nome_do_simbolo(idx), linha);
                        usada[endereco] = 1;   /* regra 4 */
                        imprimir_nome("CRVL", nome_do_simbolo(idx));
                        tipo = tipo_da_variavel(endereco);
                  }
                  break;
            }
            default:
                  erro_sintatico(lookahead, "expressao");
      }
      return tipo;
}

/*
 * conferir_variaveis_nao_usadas
 * ------------------------------------------------------------
 * Regra 4: toda variavel declarada e com valor atribuido deve ser usada.
 * Chamada no fim de programa(). Parametros nao contam como atribuidos.
 * O erro aponta a linha da declaracao e o nome da variavel.
 */
void conferir_variaveis_nao_usadas(void)
{
      for (TNo *atual = minitabela; atual != NULL; atual = atual->prox)
      {
            int e = atual->endereco;
            if (atribuida[e] && !usada[e])
            {
                  char mensagem[80];
                  snprintf(mensagem, sizeof mensagem,
                           "variavel %s recebe valor mas nunca e usada", atual->ID);
                  erro_semantico(linha_decl[e], mensagem);
            }
      }
}

/*
 * programa -> ALGORITMO STRING declaracoes INICIO comandos FIMALGORITMO
 * ------------------------------------------------------------
 * Simbolo inicial. O nome do algoritmo vem entre aspas (STRING).
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
 * O AMEM fica depois de declaracoes() porque so ali todas as secoes var
 * foram lidas (podem vir depois de uma rotina).
 */
void programa(void)
{
      imprimir("INPP");
      int rotulo_principal = proximo_rotulo();
      imprimir_desvio("DSVS", rotulo_principal);
      consumir(TOKEN_ALGORITMO);
      consumir(TOKEN_STRING);
      declaracoes();
      imprimir_rotulo(rotulo_principal);
      imprimir_inteiro("AMEM", total_posicoes);
      consumir(TOKEN_INICIO);
      comandos();
      consumir(TOKEN_FIMALGORITMO);
      conferir_variaveis_nao_usadas();
      imprimir("PARA");
}

/*
 * declaracoes -> { secao_var | procedimento | funcao }
 * ------------------------------------------------------------
 * Aceita as tres formas em qualquer ordem, ate um token que nao inicia
 * nenhuma delas (normalmente o INICIO).
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
 * A secao pode estar vazia (ha exemplos assim no anexo).
 *
 * Regra 2: o laco tambem entra numa palavra reservada que nao encerra a
 * secao, como o "se" de "se: inteiro"; reservada_na_declaracao decide
 * se era mesmo uma declaracao.
 */
void secao_var(void)
{
      consumir(TOKEN_VAR);
      while (lookahead.type == TOKEN_ID
             || (eh_palavra_reservada(lookahead.type)
                 && !encerra_secao_var(lookahead.type)))
      {
            if (lookahead.type != TOKEN_ID)
                  reservada_na_declaracao();   /* sempre da erro */
            decl_var();
      }
}

/*
 * encerra_secao_var
 * ------------------------------------------------------------
 * Diz se o token e uma das palavras reservadas que podem vir DEPOIS de
 * uma secao var (o FOLLOW de secao_var): VAR, PROCEDIMENTO, FUNCAO ou
 * INICIO. Elas encerram a secao normalmente.
 *
 * Parametro: t - tipo do token
 * Retorno:   1 se o token encerra a secao var, 0 caso contrario.
 */
int encerra_secao_var(TokenNome t)
{
      return t == TOKEN_VAR || t == TOKEN_PROCEDIMENTO
             || t == TOKEN_FUNCAO || t == TOKEN_INICIO;
}

/*
 * reservada_na_declaracao
 * ------------------------------------------------------------
 * Chamada por secao_var quando uma linha comeca com palavra reservada.
 * Com um token de lookahead so, nao da para saber se e uma declaracao
 * ("se: inteiro") ou se faltou o INICIO ("escreval(x)"); por isso a
 * palavra e consumida e o token SEGUINTE decide:
 *
 *   ":" ou "," ... declaracao: erro semantico (regra 2)
 *   outro ........ faltou o INICIO: erro sintatico
 *
 * Nos dois casos a funcao encerra o programa.
 */
void reservada_na_declaracao(void)
{
      Token reservada = lookahead;
      next_token();

      if (lookahead.type == TOKEN_DOIS_PONTOS || lookahead.type == TOKEN_VIRGULA)
            erro_semantico(reservada.line,
                          "palavra reservada nao pode ser usada como nome de variavel");

      erro_sintatico(reservada, "INICIO");
}

/*
 * registrar_var
 * ------------------------------------------------------------
 * Coloca uma variavel (ou parametro) na minitabela, conferindo antes
 * que o nome nao e de uma rotina; inserir_var_semant confere as outras
 * variaveis (regra 3). Usada por decl_var e parametro.
 *
 * Parametros: idx              - indice do nome na tabela_simbolos
 *             tipo_basico_lido - tipo basico declarado (TOKEN_INTEIRO, ...)
 *             linha            - linha do nome, para as mensagens de erro
 */
void registrar_var(int idx, TokenNome tipo_basico_lido, int linha)
{
      if (buscar_rotina(idx) != NULL)
            erro_semantico(linha, "nome ja usado por uma rotina");
      inserir_var_semant(nome_do_simbolo(idx), tipo_basico_lido, linha);

      /* inserir_var_semant acabou de dar o endereco prox_endereco - 1 */
      linha_decl[prox_endereco - 1] = linha;
}

/*
 * decl_var -> lista_ids ":" tipo
 * ------------------------------------------------------------
 * Uma linha de declaracao, como "nome, sobrenome: caractere".
 *
 * Os nomes vem antes do tipo, entao lista_ids os guarda em vetores
 * locais e eles so entram na minitabela depois do tipo. O numero de
 * nomes vezes as posicoes de cada um soma no total_posicoes (AMEM).
 *
 * Verificacao semantica: regra 3 (nome unico), em registrar_var.
 */
void decl_var(void)
{
      int indices[MAX_IDS_DECL];
      int linhas[MAX_IDS_DECL];
      TokenNome tipo_basico_lido;

      /* parte sintatica: lista de ids, ':' e o tipo */
      int contagem_ids = lista_ids(indices, linhas);
      consumir(TOKEN_DOIS_PONTOS);
      int posicoes = tipo(&tipo_basico_lido);

      /* parte semantica: so agora o tipo e conhecido */
      for (int i = 0; i < contagem_ids; i++)
            registrar_var(indices[i], tipo_basico_lido, linhas[i]);

      total_posicoes += contagem_ids * posicoes;
}

/*
 * registrar_id
 * ------------------------------------------------------------
 * Guarda o indice e a linha do ID corrente nos vetores de lista_ids e
 * consome o ID, conferindo antes o limite dos vetores.
 *
 * Verificacao semantica: regra 2 (palavra reservada como nome),
 * inclusive depois de virgula ("a, se: inteiro").
 *
 * Parametros: indices      - indice de cada nome na tabela_simbolos
 *             linhas       - linha de cada nome, para mensagens de erro
 *             contagem_ids - posicao onde o ID corrente sera guardado
 */
void registrar_id(int indices[], int linhas[], int contagem_ids)
{
      recusar_palavra_reservada();

      if (contagem_ids >= MAX_IDS_DECL)
            erro_semantico(lookahead.line, "muitas variaveis numa mesma declaracao");

      /* indice e linha guardados ANTES de consumir o ID */
      indices[contagem_ids] = lookahead.attribute.table_index;
      linhas[contagem_ids] = lookahead.line;
      consumir(TOKEN_ID);
}

/*
 * lista_ids -> ID { "," ID }
 * ------------------------------------------------------------
 * Um ou mais identificadores separados por virgula, guardados para que
 * decl_var os insira na minitabela depois de ler o tipo.
 *
 * Parametros: indices - recebe o indice de cada nome na tabela_simbolos
 *             linhas  - recebe a linha de cada nome
 * Retorno:    quantidade de identificadores lidos.
 */
int lista_ids(int indices[], int linhas[])
{
      int contagem_ids = 0;

      registrar_id(indices, linhas, contagem_ids);
      contagem_ids++;

      while (lookahead.type == TOKEN_VIRGULA)
      {
            consumir(TOKEN_VIRGULA);
            registrar_id(indices, linhas, contagem_ids);
            contagem_ids++;
      }

      return contagem_ids;
}

/*
 * tipo -> tipo_basico | VETOR "[" NUM_INT ".." NUM_INT "]" DE tipo_basico
 * ------------------------------------------------------------
 * Se comecar com VETOR, consome a faixa e o DE; os dois casos terminam
 * num tipo basico (num vetor, o do elemento), devolvido pelo ponteiro,
 * porque o retorno ja e usado pelas posicoes.

 *
 * Parametro: tipo_basico_lido - recebe o tipo basico lido (TOKEN_INTEIRO,
 *                              TOKEN_REAL, TOKEN_CARACTERE ou TOKEN_LOGICO)
 * Retorno:   posicoes de memoria ocupadas por variavel deste tipo: 1 para
 *            tipo basico e fim - inicio + 1 para vetor (o +1 conta as
 *            duas pontas: [1..4] tem 4 posicoes).
 */
int tipo(TokenNome *tipo_basico_lido)
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
      *tipo_basico_lido = tipo_basico();
      return num_posicoes;
}

/*
 * tipo_basico -> INTEIRO | REAL | CARACTERE | LOGICO
 * ------------------------------------------------------------
 * Os quatro tipos primitivos do MiniVisualg.
 *
 * O tipo e guardado no inicio e devolvido num unico return no fim:
 * como o compilador nao sabe que erro_sintatico nunca retorna, um return
 * so dentro dos cases geraria o warning de funcao sem retorno.
 *
 * Retorno: o tipo lido (TOKEN_INTEIRO, TOKEN_REAL, TOKEN_CARACTERE ou
 *          TOKEN_LOGICO), usado nos parametros e no retorno de funcoes.
 */
TokenNome tipo_basico(void)
{
      TokenNome tipo_lido = lookahead.type;
      switch (lookahead.type)
      {
            case TOKEN_INTEIRO:
            case TOKEN_REAL:
            case TOKEN_CARACTERE:
            case TOKEN_LOGICO:
                  next_token();
                  break;
            default:
                  erro_sintatico(lookahead, "tipo (inteiro, real, caractere ou logico)");
      }
      return tipo_lido;
}

/*
 * inicia_comando
 * ------------------------------------------------------------
 * Conjunto FIRST de 'comando': diz se um token pode iniciar um comando,
 * o que permite a comandos() saber onde a lista termina.
 *
 * Parametro: t - tipo do token a testar
 * Retorno:   1 se o token inicia um comando, 0 caso contrario.
 */
int inicia_comando(TokenNome t)
{
      return t == TOKEN_ID || t == TOKEN_SE || t == TOKEN_PARA
             || t == TOKEN_ENQUANTO || t == TOKEN_LEIA || t == TOKEN_ESCREVA
             || t == TOKEN_ESCREVAL || t == TOKEN_RETORNE;
}

/*
 * comandos -> { comando }
 * ------------------------------------------------------------
 * Sequencia de comandos, possivelmente vazia.
 */
void comandos(void)
{
      while (inicia_comando(lookahead.type))
            comando();
}

/*
 * comando -> cmd_id | cmd_se | cmd_para | cmd_enquanto
 *          | cmd_leia | cmd_escreva | cmd_retorne
 * ------------------------------------------------------------
 * Escolhe a producao pelo token atual (cada comando tem um token
 * inicial proprio).
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
            default:             erro_sintatico(lookahead, "comando");
      }
}

/*
 * cmd_escreva -> ( ESCREVA | ESCREVAL ) "(" argumentos ")"
 * ------------------------------------------------------------
 * Os dois comandos so diferem na quebra de linha, que a MEPA nao tem,
 * entao geram o mesmo codigo: para cada argumento, <expr> IMPR.
 *
 * O laco e proprio, e nao argumentos(), porque o IMPR vai depois de
 * cada argumento. Um texto sozinho nao empilha nada e nao recebe IMPR;
 * texto dentro de uma expressao maior e recurso nao suportado.
 */
void cmd_escreva(void)
{
      next_token();   /* comando() ja conferiu que e ESCREVA ou ESCREVAL */
      consumir(TOKEN_ABRE_PAR);

      do
      {
            if (lookahead.type == TOKEN_VIRGULA)
                  consumir(TOKEN_VIRGULA);

            /* nenhum operador produz caractere, entao texto no inicio e
             * resultado caractere significam o texto sozinho */
            int eh_texto = (lookahead.type == TOKEN_STRING);
            int linha = lookahead.line;
            int textos_antes = textos_sem_codigo;
            TokenNome tipo = expr();
            if (!(eh_texto && tipo == TOKEN_CARACTERE))
            {
                  recusar_texto_sem_codigo(textos_antes, linha);
                  imprimir("IMPR");
            }
      } while (lookahead.type == TOKEN_VIRGULA);

      consumir(TOKEN_FECHA_PAR);
}

/*
 * cmd_id   -> ID resto_id
 * resto_id -> "<-" expr | "[" expr "]" "<-" expr | "(" argumentos ")"
 *           | vazio
 * ------------------------------------------------------------
 * Comando que comeca com identificador: atribuicao, atribuicao a vetor
 * ou chamada de procedimento, decidida pelo token depois do ID. O caso
 * vazio e o procedimento sem parametros, chamado sem parenteses.
 *
 * Codigo gerado:
 *    x <- expr .......... <expr> ARMZ x
 *    p(args) / p ........ <args> ARMZ params CHPR Lp
 *    v[i] <- expr ....... nao suportado (vetores)
 *
 * Verificacoes semanticas: regra 1 (destino declarado), regra 5 (o valor
 * cabe no tipo da variavel), regra 8 (funcao usada como procedimento) e
 * identificador que nao e procedimento usado como comando.
 */
void cmd_id(void)
{
      int idx = lookahead.attribute.table_index;
      int linha = lookahead.line;
      consumir(TOKEN_ID);
      if (lookahead.type == TOKEN_ATRIB)
      {
            consumir(TOKEN_ATRIB);
            /* o destino e conferido antes da expressao, que vem depois */
            int endereco = busca_tabela_simbolos(nome_do_simbolo(idx), linha);
            atribuida[endereco] = 1;   /* regra 4 */
            int textos_antes = textos_sem_codigo;
            TokenNome tipo_expr = expr();

            /* regra 5: o valor tem de caber no tipo da variavel */
            if (!tipo_atribuivel(tipo_da_variavel(endereco), tipo_expr))
                  erro_semantico(linha, "tipo incompativel na atribuicao");

            /* nome <- "abc": o ARMZ ficaria sem valor na pilha */
            recusar_texto_sem_codigo(textos_antes, linha);

            imprimir_nome("ARMZ", nome_do_simbolo(idx));
      }
      else if (lookahead.type == TOKEN_ABRE_COL)
      {
            nao_suportado(linha, "vetores");
            consumir(TOKEN_ABRE_COL);
            expr();
            consumir(TOKEN_FECHA_COL);
            consumir(TOKEN_ATRIB);
            expr();
      }
      else if (lookahead.type == TOKEN_ABRE_PAR)
      {
            Rotina *r = buscar_rotina(idx);
            if (r == NULL)
                  erro_semantico(linha, "identificador nao e um procedimento");
            if (r->ehFuncao)
                  erro_semantico(linha, "funcao nao pode ser usada como procedimento");

            consumir(TOKEN_ABRE_PAR);

            TokenNome tipos[MAX_PARAMS];
            int textos_antes = textos_sem_codigo;
            int num_args = argumentos(tipos);
            consumir(TOKEN_FECHA_PAR);
            gerar_chamada(r, num_args, linha, tipos);
            recusar_texto_sem_codigo(textos_antes, linha);
      }
      else
      {
            Rotina *r = buscar_rotina(idx);
            if (r == NULL)
                  erro_semantico(linha, "identificador nao e um procedimento");
            if (r->ehFuncao)
                  erro_semantico(linha, "funcao nao pode ser usada como procedimento");

            gerar_chamada(r, 0, linha, NULL);
      }
}

/*
 * cmd_leia -> LEIA "(" variavel ")"
 * ------------------------------------------------------------
 * O argumento do leia e uma variavel, e nao uma expressao, por isso
 * chama variavel() e nao expr().
 *
 * Codigo gerado: LEIT, ARMZ variavel.
 */
void cmd_leia(void)
{
      consumir(TOKEN_LEIA);
      consumir(TOKEN_ABRE_PAR);
      int idx = variavel();
      imprimir("LEIT");
      imprimir_nome("ARMZ", nome_do_simbolo(idx));
      consumir(TOKEN_FECHA_PAR);
}

/*
 * variavel -> ID [ "[" expr "]" ]
 * ------------------------------------------------------------
 * Variavel simples ou elemento de vetor (nao suportado na geracao). Nao
 * gera codigo: devolve o nome para quem chamou.
 *
 * Verificacao semantica: regra 1 (variavel declarada).
 *
 * Retorno: indice do nome da variavel na tabela_simbolos.
 */
int variavel(void)
{
      int idx = lookahead.attribute.table_index;
      int linha = lookahead.line;
      consumir(TOKEN_ID);
      int endereco = busca_tabela_simbolos(nome_do_simbolo(idx), linha);
      atribuida[endereco] = 1;   /* regra 4: variavel() so e usada pelo leia */
      if (lookahead.type == TOKEN_ABRE_COL)
      {
            nao_suportado(linha, "vetores");
            consumir(TOKEN_ABRE_COL);
            expr();
            consumir(TOKEN_FECHA_COL);
      }
      return idx;
}

/*
 * cmd_se -> SE expr ENTAO comandos [ SENAO comandos ] FIMSE
 * ------------------------------------------------------------
 * O FIMSE obrigatorio elimina a ambiguidade do "senao pendurado".
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
 * Os dois rotulos sao pedidos no inicio (ver proximo_rotulo); sem senao,
 * o segundo fica sem uso.
 *
 * Verificacao semantica: regra 5, a condicao tem de ser logica.
 */
void cmd_se(void)
{
      int rotulo_senao = proximo_rotulo();
      int rotulo_fim = proximo_rotulo();

      consumir(TOKEN_SE);
      int linha = lookahead.line;
      if (expr() != TOKEN_LOGICO)
            erro_semantico(linha, "condicao do se deve ser logica");
      imprimir_desvio("DSVF", rotulo_senao);
      consumir(TOKEN_ENTAO);
      comandos();
      if (lookahead.type == TOKEN_SENAO)
      {
            imprimir_desvio("DSVS", rotulo_fim);
            imprimir_rotulo(rotulo_senao);
            consumir(TOKEN_SENAO);
            comandos();
            imprimir_rotulo(rotulo_fim);
      }
      else
      {
            /* sem senao, o rotulo do senao e o proprio fim do se */
            imprimir_rotulo(rotulo_senao);
      }

      consumir(TOKEN_FIMSE);
}

/*
 * cmd_para -> PARA ID DE expr ATE expr [ PASSO expr ] FACA
 *             comandos FIMPARA
 * ------------------------------------------------------------
 * O PASSO e opcional e pode ser negativo ("passo -2" chega como SUB
 * seguido de NUM_INT).
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
 * No fonte o passo vem ANTES do corpo, mas o incremento sai DEPOIS dele.
 * Por isso o passo e uma constante inteira, lida direto do token, e a
 * comparacao so e emitida depois dele: o sinal escolhe CMEG ou CMAG.
 *
 * Verificacoes semanticas: regra 1 (variavel declarada), regra 5
 * (variavel, valor inicial e limite inteiros) e passo nao constante ou
 * zero (o laco nunca terminaria).
 */
void cmd_para(void)
{
      int rotulo_inicio = proximo_rotulo();
      int rotulo_fim = proximo_rotulo();

      consumir(TOKEN_PARA);
      int idx = lookahead.attribute.table_index;
      int linha = lookahead.line;
      consumir(TOKEN_ID);
      int endereco = busca_tabela_simbolos(nome_do_simbolo(idx), linha);

      /* regra 5: o contador do laco e inteiro (o passo tambem e) */
      if (tipo_da_variavel(endereco) != TOKEN_INTEIRO)
            erro_semantico(linha, "variavel de controle do para deve ser inteira");

      /* regra 4: o laco atribui a variavel e tambem a le na comparacao */
      atribuida[endereco] = 1;
      usada[endereco] = 1;

      consumir(TOKEN_DE);
      int linha_inicio = lookahead.line;
      if (expr() != TOKEN_INTEIRO)
            erro_semantico(linha_inicio, "valor inicial do para deve ser inteiro");
      imprimir_nome("ARMZ", nome_do_simbolo(idx));

      consumir(TOKEN_ATE);
      imprimir_rotulo(rotulo_inicio);
      imprimir_nome("CRVL", nome_do_simbolo(idx));
      int linha_limite = lookahead.line;
      if (expr() != TOKEN_INTEIRO)
            erro_semantico(linha_limite, "limite do para deve ser inteiro");

      int passo = 1;
      if (lookahead.type == TOKEN_PASSO)
      {
            consumir(TOKEN_PASSO);
            int linha_passo = lookahead.line;

            int negativo = 0;
            if (lookahead.type == TOKEN_SUB)
            {
                  consumir(TOKEN_SUB);
                  negativo = 1;
            }

            if (lookahead.type != TOKEN_NUM_INT)
                  erro_semantico(linha_passo, "passo do para deve ser uma constante inteira");

            passo = lookahead.attribute.int_value;
            consumir(TOKEN_NUM_INT);

            if (negativo)
                  passo = -passo;

            if (passo == 0)
                  erro_semantico(linha_passo, "passo do para nao pode ser zero");
      }

      if (passo > 0)
            imprimir("CMEG");
      else
            imprimir("CMAG");
      imprimir_desvio("DSVF", rotulo_fim);

      consumir(TOKEN_FACA);
      comandos();

      imprimir_nome("CRVL", nome_do_simbolo(idx));
      imprimir_inteiro("CRCT", passo);
      imprimir("SOMA");
      imprimir_nome("ARMZ", nome_do_simbolo(idx));

      imprimir_desvio("DSVS", rotulo_inicio);
      imprimir_rotulo(rotulo_fim);

      consumir(TOKEN_FIMPARA);
}

/*
 * cmd_enquanto -> ENQUANTO expr FACA comandos FIMENQUANTO
 * ------------------------------------------------------------
 * Codigo gerado:
 *
 *    Li: NADA              <- antes da condicao: ela e recalculada a
 *        <expr>               cada volta
 *        DSVF Lf
 *        <corpo>
 *        DSVS Li
 *    Lf: NADA
 *
 * Verificacao semantica: regra 5, a condicao tem de ser logica.
 */
void cmd_enquanto(void)
{
      int rotulo_inicio = proximo_rotulo();
      int rotulo_fim = proximo_rotulo();

      consumir(TOKEN_ENQUANTO);
      imprimir_rotulo(rotulo_inicio);
      int linha = lookahead.line;
      if (expr() != TOKEN_LOGICO)
            erro_semantico(linha, "condicao do enquanto deve ser logica");
      imprimir_desvio("DSVF", rotulo_fim);
      consumir(TOKEN_FACA);
      comandos();
      imprimir_desvio("DSVS", rotulo_inicio);
      imprimir_rotulo(rotulo_fim);
      consumir(TOKEN_FIMENQUANTO);
}

/*
 * cmd_retorne -> RETORNE expr
 * ------------------------------------------------------------
 * Retorno de funcao. O valor vai para a posicao com o nome da funcao
 * (quem chamou le dali com CRVL), e o RTPR sai da funcao na hora.
 *
 * Codigo gerado: <expr> ARMZ nome_da_funcao RTPR 1,0
 *
 * Verificacoes semanticas: retorne so dentro de uma funcao (conferido
 * primeiro, porque fora dela rotina_atual e NULL) e regra 9, o valor tem
 * de caber no tipo de retorno declarado (tipo_atribuivel).
 */
void cmd_retorne(void)
{
      int linha = lookahead.line;
      consumir(TOKEN_RETORNE);

      if (rotina_atual == NULL || !rotina_atual->ehFuncao)
            erro_semantico(linha, "retorne so pode ser usado dentro de uma funcao");

      /* marca para funcao() saber que o corpo devolve algum valor */
      rotina_atual->temRetorne = 1;

      /* regra 9: o valor tem de caber no tipo declarado da funcao */
      int textos_antes = textos_sem_codigo;
      TokenNome tipo = expr();
      if (!tipo_atribuivel(rotina_atual->tipoRetorno, tipo))
            erro_semantico(linha, "tipo do retorno incompativel com a declaracao da funcao");
      recusar_texto_sem_codigo(textos_antes, linha);

      imprimir_nome("ARMZ", nome_do_simbolo(rotina_atual->nome));
      imprimir_dois_inteiros("RTPR", 1, 0);
}

/*
 * procedimento -> PROCEDIMENTO ID [ "(" parametros ")" ]
 *                 INICIO comandos FIMPROCEDIMENTO
 * ------------------------------------------------------------
 * Sem parametros, nao leva parenteses na declaracao nem na chamada.
 *
 * Codigo gerado (entre o DSVS e o AMEM do programa principal):
 *
 *    Lp: NADA              <- rotulo guardado na tabela, usado pelo CHPR
 *        ENPR 1
 *        <corpo>
 *        RTPR 1,0
 *
 * ENPR e RTPR vem da MEPA do livro de Kowaltowski (docs/etapa2.md,
 * secao 8.4). O nivel e sempre 1 e o RTPR descarta 0 posicoes, porque
 * os parametros ficam em posicoes com nome, e nao na pilha.
 *
 * Verificacao semantica: regra 3 (nome nao repete outra rotina nem uma
 * variavel).
 */
void procedimento(void)
{
      consumir(TOKEN_PROCEDIMENTO);
      int idx = lookahead.attribute.table_index;
      int linha = lookahead.line;
      consumir(TOKEN_ID);

      /* regra 3, antes de inserir (senao a rotina se acharia) */
      if (buscar_rotina(idx) != NULL)
            erro_semantico(linha, "rotina declarada mais de uma vez");
      if (existe_na_minitabela(nome_do_simbolo(idx)))
            erro_semantico(linha, "nome ja usado por uma variavel");

      Rotina *r = inserir_rotina(idx, 0);
      r->rotulo = proximo_rotulo();
      rotina_atual = r;

      imprimir_rotulo(r->rotulo);
      imprimir_inteiro("ENPR", 1);

      if (lookahead.type == TOKEN_ABRE_PAR)
      {
            consumir(TOKEN_ABRE_PAR);
            parametros(r);
            consumir(TOKEN_FECHA_PAR);
      }
      consumir(TOKEN_INICIO);
      comandos();

      imprimir_dois_inteiros("RTPR", 1, 0);
      rotina_atual = NULL;

      consumir(TOKEN_FIMPROCEDIMENTO);
}

/*
 * funcao -> FUNCAO ID "(" parametros ")" ":" tipo_basico
 *           INICIO comandos FIMFUNCAO
 * ------------------------------------------------------------
 * Gera o mesmo codigo do procedimento, mas reserva uma posicao a mais,
 * com o nome da funcao, para o valor de retorno.
 *
 * Verificacoes semanticas: regra 3 e regra 9, o corpo precisa ter algum
 * retorne. A conferencia e so de presenca: um retorne dentro de um se ja
 * conta, porque o compilador nao analisa os caminhos de execucao.
 */
void funcao(void)
{
      consumir(TOKEN_FUNCAO);
      int idx = lookahead.attribute.table_index;
      int linha = lookahead.line;
      consumir(TOKEN_ID);

      /* regra 3: mesma conferencia do procedimento */
      if (buscar_rotina(idx) != NULL)
            erro_semantico(linha, "rotina declarada mais de uma vez");
      if (existe_na_minitabela(nome_do_simbolo(idx)))
            erro_semantico(linha, "nome ja usado por uma variavel");

      Rotina *r = inserir_rotina(idx, 1);
      r->rotulo = proximo_rotulo();
      rotina_atual = r;

      total_posicoes++;

      imprimir_rotulo(r->rotulo);
      imprimir_inteiro("ENPR", 1);

      consumir(TOKEN_ABRE_PAR);
      parametros(r);
      consumir(TOKEN_FECHA_PAR);
      consumir(TOKEN_DOIS_PONTOS);
      r->tipoRetorno = tipo_basico();

      consumir(TOKEN_INICIO);
      comandos();

      if (!r->temRetorne)
            erro_semantico(linha, "funcao sem retorne");

      imprimir_dois_inteiros("RTPR", 1, 0);
      rotina_atual = NULL;

      consumir(TOKEN_FIMFUNCAO);
}

/*
 * parametros -> parametro { "," parametro }
 * ------------------------------------------------------------
 * Parametro: r - rotina que esta sendo declarada
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
 * Um parametro, como "a: inteiro". Registra nome e tipo na rotina e
 * reserva uma posicao no AMEM: o parametro e uma posicao com nome, como
 * uma variavel, e por isso tambem entra na minitabela. Como os nomes sao
 * globais, dois parametros iguais (mesmo em rotinas diferentes) sao erro.
 *
 * Verificacoes semanticas: regras 2 e 3.
 *
 * Parametro: r - rotina a que o parametro pertence
 */
void parametro(Rotina *r)
{
      recusar_palavra_reservada();

      int idx = lookahead.attribute.table_index;
      int linha = lookahead.line;
      consumir(TOKEN_ID);
      consumir(TOKEN_DOIS_PONTOS);
      TokenNome tipo_param = tipo_basico();
      adicionar_parametro(r, idx, tipo_param);
      registrar_var(idx, tipo_param, linha);
      total_posicoes++;
}

/* ============================================================
 *  8. MAIN
 * ============================================================ */

/*
 * main
 * ------------------------------------------------------------
 * Carrega o fonte, abre as saidas e dispara a compilacao. O primeiro
 * next_token() carrega o lookahead inicial; depois de programa(), o
 * teste de TOKEN_EOF garante que nada sobrou depois do fimalgoritmo.
 *
 * Retorno: 0 sem erros, 1 em erro de uso ou de arquivo (os outros erros
 *          encerram o programa dentro das funcoes de erro).
 */
int main(int argc, char *argv[])
{
      if (argc < 2)
      {
            printf("Uso: %s <arquivo_fonte.alg>\n", argv[0]);
            return 1;
      }

      if (!carregar_arquivo(argv[1]))
      {
            printf("Erro ao abrir o arquivo %s\n", argv[1]);
            return 1;
      }

      arquivo_saida = fopen(ARQUIVO_SAIDA, "w");
      if (arquivo_saida == NULL)
      {
            printf("Erro ao criar %s\n", ARQUIVO_SAIDA);
            free(buffer);
            return 1;
      }

      arquivo_mepa = fopen(ARQUIVO_MEPA, "w");
      if (arquivo_mepa == NULL)
      {
            printf("Erro ao criar %s\n", ARQUIVO_MEPA);
            fclose(arquivo_saida);
            free(buffer);
            return 1;
      }

      /* todas as etapas numa unica passagem, dirigida pelo parser */
      next_token();
      programa();

      if (lookahead.type != TOKEN_EOF)
            erro_sintatico(lookahead, "fim do arquivo");

      /* chegar aqui significa que todas as analises passaram */

      printf("Compilacao concluida sem erros. Codigo MEPA gerado em %s\n",
             ARQUIVO_MEPA);
      fprintf(arquivo_saida,
              "Compilacao concluida sem erros. Codigo MEPA gerado em %s\n",
              ARQUIVO_MEPA);

      fclose(arquivo_saida);
      fclose(arquivo_mepa);
      free(buffer);
      liberar_tabela_simbolos();
      return 0;
}
