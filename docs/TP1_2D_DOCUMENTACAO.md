# TP1 2D – Documentação do Projeto Tree TP1

Este documento registra todo o trabalho realizado sobre o fork do projeto **Khronos Vulkan Samples** para atender ao **TP1 (2D)** usando os dados disponibilizados pelo professor. A base continua sendo o repositório oficial, mas a amostra `tree_tp1` foi desenvolvida do zero para interpretar, animar e renderizar as árvores arteriais contidas em `assets/TP_CCO_Pacote_Dados/TP1_2D`.

---

## 1. Origem do projeto

- **Fork do Vulkan-Samples**: o repositório contém a infraestrutura completa da Arm/Khronos. Aproveitamos o framework (`framework/`, `components/`), o aplicativo Android (`app/`) e o mecanismo de registro dinâmico de samples (`samples/`).
- **Nova amostra**: `samples/api/tree_tp1` foi criada a partir de `triangle_play`, porém todo o conteúdo foi substituído para atender os requisitos do TP1.
- **Assets acadêmicos**: foram adicionados ao diretório `assets/TP_CCO_Pacote_Dados/TP1_2D` e são carregados diretamente pela amostra.

---

## 2. Estrutura geral e arquivos relevantes

| Caminho | Função |
| --- | --- |
| `samples/api/tree_tp1/tree_tp1.cpp` | C++ principal da amostra. Faz parsing dos VTK, controla buffers, animações e desenha a árvore. |
| `samples/api/tree_tp1/tree_tp1.h` | Declara as estruturas (segmentos, vértices, UBO) e parâmetros expostos no overlay. |
| `shaders/tree_tp1/glsl/tree.vert` | Vertex shader: aplica transformações, envia dados para o fragment shader. |
| `shaders/tree_tp1/glsl/tree.frag` | Fragment shader: gera os “tubos” com suavização por SDF e coloração baseada no raio. |
| `docs/TP1_2D_DOCUMENTACAO.md` | Este documento, com o passo a passo completo. |
| `assets/TP_CCO_Pacote_Dados/TP1_2D/...` | Dados VTK do professor (Nterm_064, 128 e 256). |

---

## 3. Dependências e arquitetura do fork

- **Vulkan SDK**: necessário para compilar e rodar no desktop (inclui headers, camadas e compiladores de shaders). No macOS basta instalar o SDK oficial e executar `source <SDK>/setup-env.sh` para configurar `VK_ICD_FILENAMES`, `VULKAN_SDK`, etc.
- **CMake**: o repositório inteiro é gerado via CMake. Cada sample declara seu `add_sample` e automaticamente é incluído no app (desktop e Android). Os scripts cuidam da compilação offline dos shaders (GLSL → SPIR-V).
- **Framework interno**:
  - `framework/`: classes utilitárias (ex.: `ApiVulkanSample`, `Camera`, sistemas de render, ImGui).
  - `components/`: implementações de alto nível (filesystem, logging, etc.).
  - `app/`: empacota tudo em executáveis (glfw para desktop, Android app para celulares).
- **Assets**:
  - `assets/` abriga texturas, modelos e, neste trabalho, os VTK do professor.
  - `shaders/` contém todos os GLSL. Cada sample define seu subdiretório (`tree_tp1/glsl`).
- **Estrutura de um sample Vulkan**:
  1. Código C++ herda de `ApiVulkanSample`, que fornece a janela, swapchain e loop de render.
  2. Métodos obrigatórios: `prepare()`, `build_command_buffers()`, `render()`, `on_update_ui_overlay()`.
  3. Recursos Vulkan (buffers, UBOs, pipelines) ficam encapsulados em utilitários (`vkb::core::BufferC`, `load_shader`, etc.).
  4. Os shaders são listados no `CMakeLists.txt` do sample para que sejam compilados antes da execução.

Esse padrão facilita criar novas amostras: basta copiar uma pasta existente, ajustar o CMake e escrever o código específico (neste projeto, todo o comportamento está em `tree_tp1.cpp/.h` e `shaders/tree_tp1/glsl`).

---

## 3. Fluxo da amostra `tree_tp1`

1. **Seleção do dataset** (UI): escolhe automaticamente os arquivos `.vtk` por Nterm e step.
2. **Parser VTK**:
   - Lê os blocos `POINTS`, `LINES` e `CELL_DATA`.
   - Monta um grafo para medir a distância de cada segmento ao tronco (usado para o afinamento dos galhos).
3. **Geração de geometria**:
   - Cada segmento vira uma cápsula com preenchimento suave (SDF), usando atributos extras (extremos 3D, raio, fator de ponta).
   - O progresso de crescimento (slider “Galhos visíveis” ou animação) define quantos segmentos serão realmente enviados para a GPU.
4. **Shaders**:
   - VS passa a posição local, extremos e raio para o FS.
   - FS calcula a distância ao eixo da cápsula, aplica smoothstep nas bordas e intensifica as cores conforme o raio.
5. **Transformações**:
   - Matriz ortográfica + rotações X/Y/Z + escala e translação para enquadramento fino.
6. **Overlay (ImGui)**:
   - Dataset, frame (`stepXXXX`), animação dos arquivos.
   - Controles de crescimento dos galhos (manual e automático).
   - Alternância entre “Raio fixo” e “Raio variável”.
   - Ajustes de cores/raios, escala e rotações.

---

## 4. Funcionalidades implementadas para o TP1

### 4.1 Parsing e organização dos dados
- Manipula qualquer arquivo VTK 2D fornecido.
- Calcula automaticamente os limites e a variação de raios.
- Ordena os segmentos do tronco para a ponta para permitir o crescimento incremental.

### 4.2 Renderização em 2D com profundidade visual
- Uso de cápsulas extrudadas em 3D, mas posicionadas no plano XY.
- Shading baseado em SDF para remover serrilhados e simular volume.
- Gradiente de cores configurado em função do raio real do segmento.

### 4.3 Transformações e projeção
- Uniform buffer contendo a matriz ortográfica composta com escala, rotações (X/Y/Z) e translação.
- Possibilidade de inclinar a árvore para dar profundidade mesmo no modo 2D.

### 4.4 Visualização incremental
- Slider “Galhos visíveis” mostra exatamente quantos segmentos estão sendo desenhados.
- “Animate growth” revela galhos automaticamente, um por vez, na ordem correta.
- Animação dos diferentes arquivos (`stepXXXX`) continua disponível, permitindo ver o crescimento fisiológico do dataset.

### 4.5 Controles adicionais
- Alternância entre raio fixo e raio variável (com faixas mínima/máxima).
- Ajuste de cores e intensidades via shader.
- Indicadores para cada dataset/step, facilitando a apresentação.

---

## 5. Build e execução

### 5.1 macOS (desktop)
```bash
source /PATH/TO/VULKAN/SDK/setup-env.sh
cmake -B build/mac -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT=macosx -DCMAKE_OSX_DEPLOYMENT_TARGET=13.3
cmake --build build/mac --config Release --target vulkan_samples -j"$(sysctl -n hw.ncpu)"
./build/mac/app/bin/Release/$(uname -m)/vulkan_samples sample tree_tp1
```

### 5.2 Android
1. Sincronize os assets (`assets/` e `shaders/`) ou deixe o Gradle/ADB copiar automaticamente.
2. Execute pelo Android Studio selecionando o sample `tree_tp1` no menu principal do app.

---

## 6. Passo a passo sugerido para apresentação

1. **Explicar a base**: mencionar que se trata do fork do Vulkan Samples e apontar onde a nova amostra foi criada.
2. **Mostrar os assets**: navegar até `assets/TP_CCO_Pacote_Dados/TP1_2D` e abrir um `.vtk` para evidenciar a estrutura.
3. **Demonstrar o app**:
   - Escolher um Nterm, avançar pelos `stepXXXX`.
   - Ativar “Animate growth” e mostrar a árvore sendo construída segmento a segmento.
   - Trocar entre “Raio fixo” e “Raio variável” destacando as diferenças.
   - Ajustar rotações/escala para mostrar a profundidade simulada.
4. **Detalhar o código**:
   - `parse_vtk_file`: mostrar como os dados são lidos.
   - `build_vertices_from_segments`: explicar geração das cápsulas + controle de crescimento.
   - `tree.vert`/`tree.frag`: comentar brevemente o shading.
5. **Concluir** mostrando como a arquitetura facilita avançar para o TP2 (os dados 3D já estão em `assets/TP_CCO_Pacote_Dados/TP2_3D`).

---

## 7. Checklist de requisitos atendidos

- [x] Leitura dos arquivos VTK (todas as variações de Nterm).
- [x] Renderização 2D com projeção ortográfica e shading suave.
- [x] Transformações completas (translação, rotação, escala).
- [x] Visualização incremental via arquivos parciais (`stepXXXX`).
- [x] Crescimento progressivo da árvore (slider + animação).
- [x] Documentação detalhada do processo.

---

## 8. Detalhamento técnico adicional

### 8.1 Formato dos dados e leitura
- **Estrutura VTK**: cada arquivo contém seções `POINTS`, `LINES` e `CELL_DATA`.
  - `POINTS`: coordenadas 3D (utilizamos apenas `x` e `y`).
  - `LINES`: define segmentos como pares de índices. É aqui que a conectividade da árvore é descrita.
  - `CELL_DATA` / `scalars raio`: armazena o raio de cada segmento.
- **Algoritmo de parsing**:
  1. Lê `POINTS` em um vetor de `glm::vec2`.
  2. Lê `LINES` em uma lista de `SegmentIndices`.
  3. Lê `CELL_DATA` em um vetor de `float`.
  4. Monta um grafo ponto-a-ponto e usa Dijkstra para calcular a distância de cada nó ao tronco.
  5. Constrói `SegmentData` (extremos, raio, `depth_factor`) e uma `render_order` que garante que os galhos principais nasçam antes das pontas.

### 8.2 Transformações 2D e API Vulkan
- **API**: Vulkan foi utilizada por já estar integrada ao projeto-base, permitir execução em Android/macOS e oferecer controle sobre buffers/pipelines.
- **Transformações geométricas**:
  - Em coordenadas homogêneas 3×3 (2D), a árvore é transformada por:
    \[
    S(s_x, s_y) =
    \begin{bmatrix}
    s_x & 0 & 0 \\
    0 & s_y & 0 \\
    0 & 0 & 1
    \end{bmatrix},
    \quad
    R_z(\theta) =
    \begin{bmatrix}
    \cos\theta & -\sin\theta & 0 \\
    \sin\theta & \cos\theta & 0 \\
    0 & 0 & 1
    \end{bmatrix},
    \quad
    T(t_x, t_y) =
    \begin{bmatrix}
    1 & 0 & t_x \\
    0 & 1 & t_y \\
    0 & 0 & 1
    \end{bmatrix}
    \]
  - As rotações adicionais que usamos (para dar sensação de profundidade) são:
    \[
    R_x(\alpha) =
    \begin{bmatrix}
    1 & 0 & 0 \\
    0 & \cos\alpha & -\sin\alpha \\
    0 & \sin\alpha & \cos\alpha
    \end{bmatrix},
    \quad
    R_y(\beta) =
    \begin{bmatrix}
    \cos\beta & 0 & \sin\beta \\
    0 & 1 & 0 \\
    -\sin\beta & 0 & \cos\beta
    \end{bmatrix}
    \]
    Elas são aplicadas antes da rotação em Z para inclinar a árvore e criar uma percepção 3D mesmo que os dados sejam planos.
  - A composição final (antes de aplicar a projeção ortográfica) segue a ordem `M = T · R_z · R_y · R_x · S · C`, onde `C` centraliza a árvore removendo o centro do bounding box.
  - A projeção ortográfica usada pelo Vulkan é obtida com `glm::ortho(left, right, bottom, top, -1, 1)` e gera a matriz `P`.
  - O UBO armazena `MVP = P * M`, que o vertex shader aplica em cada vértice.

