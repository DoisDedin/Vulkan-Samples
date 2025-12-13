# TP1 2D - Documentação Técnica

> Base do projeto: `/Users/joaovitorcotta/AndroidStudioProjects/Vulkan-SamplesTwo`

## 1. Visão geral da base Vulkan-SamplesTwo
- **Framework** (`framework/` e `components/`): abstraem criação de dispositivos Vulkan, swapchain, alocação de buffers e carregamento de shaders. As amostras herdam de classes do diretório `framework/core`.
- **App Android** (`app/`): empacota as amostras para Android/NDK. O módulo `app/src/main/cpp` registra as amostras expostas por `samples/`.
- **Samples** (`samples/api/*`): cada pasta possui um `CMakeLists.txt` (ex.: `samples/api/triangle_play/CMakeLists.txt`) que registra a demonstração via `add_sample`. O código C++ fica em `samples/api/<sample>/<sample>.cpp`.
- **Assets** (`assets/`): arquivos acessíveis em tempo de execução (shaders, texturas e, neste caso, os dados VTK compartilhados pelo professor).

> **Sample-alvo**: `samples/api/triangle_play/` já inicia pipeline Vulkan simples (triângulos animados) e serve como ponto de partida para o TP1. Podemos duplicar/renomear essa amostra para evoluir os requisitos sem afetar o restante do pacote.

## 2. Estrutura de assets para o TP1
```
assets/TP_CCO_Pacote_Dados/
 ├─ TP1_2D/
 │   ├─ Nterm_064/
 │   │   └─ tree2D_Nterm0064_stepXXXX.vtk
 │   ├─ Nterm_128/ ...
 │   └─ Nterm_256/ ...
 └─ TP2_3D/ (mantido para a fase seguinte)
```
- Cada pasta `Nterm_xxx` guarda uma sequência de arquivos `.vtk` numerados (`step0008`, `step0016`, ...), permitindo animar o crescimento incremental.
- Formato observado em `tree2D_Nterm0064_step0008.vtk`:
  - Bloco `POINTS`: lista de vértices 3D (z = 0 para dados 2D).
  - Bloco `LINES`: pares índices → segmentos.
  - Bloco `CELL_DATA` com `scalars raio`: raio por segmento, usado para colorir a árvore.

## 3. Mapeamento dos requisitos do TP1

| Requisito | Estratégia na base atual |
|-----------|--------------------------|
| 1. Leitura VTK 2D | Adicionar loader em `samples/api/triangle_play/triangle_play.cpp` (ou nova amostra) usando `std::ifstream`. Parsear cabeçalho, vetor de pontos, conectividade e array de raios. Organizar em structs `Segment {glm::vec2 a, b; float radius; }`. |
| 2. Renderização 2D | Substituir os triângulos por linhas/tubos renderizados com pipeline de linhas (`VK_PRIMITIVE_TOPOLOGY_LINE_LIST`) ou instanciando cilindros 2D. Buffers (`VertexBuffer`, `IndexBuffer`) já são suportados pela infraestrutura. |
| 3. Transformações 2D | Adicionar uniform buffer ou push constants com matriz 3x3/4x4. Utilizar `glm::mat4` para translação, rotação e escala. Mapear entradas de toque/gestos em Android através do `InputEvent` já exposto pelo framework. |
| 4. Projeção ortográfica | Configurar matriz `glm::ortho(left, right, bottom, top)` baseada na janela e na amplitude dos dados. As matrizes entram no mesmo uniform buffer das transformações. |
| 5. Visualização incremental | Guardar vetor de caminhos VTK e expor UI simples (ex.: ciclo automático) que troca os dados após `Δt`. Reusar loader para cada arquivo e atualizar buffers em GPU. |
| 6. Bônus (recorte) | Implementar automaticamente no CPU (ex.: Liang-Barsky) antes de enviar ao GPU. Marcar segmentos recortados com cor diferenciada para visualização. |

## 4. Fluxo proposto do aplicativo
1. **Seleção do dataset**: escolher `Nterm_064`, `128` ou `256`. Armazenar caminho base em um JSON simples (ex.: `assets/TP_CCO_Pacote_Dados/index.json`) ou hardcode inicial.
2. **Carregamento**:
   - Ler arquivo VTK selecionado.
   - Converter listas em buffers:
     ```
     struct Vertex2D { glm::vec2 pos; float radius; };
     ```
   - Para colorização, enviar tabela `radius_min/max` e interpolar cores no shader fragment (`triangle_play/glsl/triangle.frag` substituído).
3. **Atualização/entrada**:
   - Listener para gestos → atualizar matriz model.
   - Timer/slider → avança para próximo arquivo e realimenta os buffers.
4. **Renderização**:
   - Pipeline Vulkan com blend habilitado para sobreposição.
   - Projeção ortográfica carregada por uniform buffer.
   - Opcional: dois passes (segmentos + máscara de recorte/borda).

## 5. Componentes a desenvolver
- `samples/api/tree_tp1/` (novo sample baseado em `triangle_play`):
  - `tree_tp1.cpp`: classe derivada de `ApiSample` contendo:
    - Loader VTK (`load_vtk(const std::string &path)`).
    - Estrutura `TreeAnimation` com vetor de caminhos e índice atual.
    - Buffers `vertex_buffer`, `index_buffer`, uniform buffer para `TransformUBO`.
  - `glsl/tree.vert` e `glsl/tree.frag`: shaders com matriz ortográfica + cor baseada no raio (usar gradiente HSV ou colormap simples).
  - `CMakeLists.txt`: apontar novos shaders e sample metadata.
- **Entrada Android**:
  - Em `app/src/main/java/.../SampleLauncherActivity`, garantir que o sample novo apareça no menu interno (o registro acontece automaticamente após `add_sample` + rebuild do catálogo).
- **Ferramentas auxiliares** (opcional):
  - Script Python/C++ pequeno em `scripts/` para verificar integridade dos arquivos VTK e gerar estatísticas (min/max de raios por Nterm).

## 6. Plano incremental sugerido
1. Clonar `samples/api/triangle_play` → `samples/api/tree_tp1`.
2. Ajustar `CMakeLists.txt` + registrar shaders.
3. Implementar parser VTK (focar em `POINTS`, `LINES`, `CELL_DATA`).
4. Criar pipelines/buffers e desenhar árvore estática.
5. Adicionar controles de transformação (gestos ou teclas debug) e projeção ortográfica.
6. Implementar animação percorrendo arquivos `stepXXXX`.
7. (Bônus) Aplicar algoritmo de recorte 2D + visualização.

## 7. Considerações para apresentação ao professor
- Destacar que todo o desenvolvimento ocorre dentro da infraestrutura oficial da Arm, garantindo compatibilidade com Android + Vulkan 1.1.
- Mostrar o diretório `assets/TP_CCO_Pacote_Dados/TP1_2D` para comprovar o uso dos dados fornecidos.
- Explicar que a modularização em samples permite evoluir facilmente para o TP2 (3D) reutilizando a mesma base.
- Documentar no relatório final:
  - Como o parser trata variabilidade de `Nterm`.
  - Estratégia de normalização das cores (gradiente).
  - Forma de interação do usuário e atalhos.
  - Resultados e capturas de tela da animação incremental.
