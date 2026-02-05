# TP2 3D – Documentação do Projeto CCO Tree 3D

UNIVERSIDADE FEDERAL DE OURO PRETO

João Vitor Cardoso dos Santos Cotta - 19.2.4069  
Bruno Jose Baeta Barbosa - 18.1.4151  
Crescêncio Gusmão Castro - 22.2.4094

Computação Gráfica - BCC327  
TP-02  
Fevereiro/2026

---

## 1. Introdução

Este trabalho utiliza **Vulkan** como API gráfica principal. A escolha se deu pelo interesse em aprofundar o entendimento de uma API de baixo nível que oferece maior controle sobre o pipeline gráfico e potencial de otimização, inclusive para cálculos matemáticos pesados que podem ser acelerados pela GPU.

Principais características e benefícios:
- **Baixa sobrecarga da CPU**: Vulkan reduz a carga no processador em comparação com APIs mais antigas, permitindo FPS mais estável.
- **Acesso “close-to-metal”**: controle explícito sobre recursos e comandos enviados à GPU.
- **Multiplataforma**: disponível em Windows, Linux, Android e outros ambientes.
- **Multithreading eficiente**: pensado para aproveitar CPUs multi-core.
- **Evolução do OpenGL**: incorpora recursos modernos que não existem no OpenGL ES.

---

## 2. Origem e base do projeto

O projeto foi construído sobre um fork do **Khronos Vulkan Samples**, que fornece infraestrutura completa para aplicações Vulkan em desktop e Android. Esse repositório inclui:
- Framework gráfico com abstrações Vulkan
- Sistema de janelas, swapchain e loop de renderização
- Integração com ImGui para interfaces interativas
- Suporte multiplataforma (desktop e Android)

A partir dessa base, foi criada a amostra:
- `samples/api/cco_tree_3d`

Essa amostra foi inicialmente baseada no sample `triangle_play`, porém todo o código foi reescrito para atender exclusivamente aos requisitos do TP2.

Os dados acadêmicos fornecidos pelo professor foram adicionados em:
- `assets/TP_CCO_Pacote_Dados/TP2_3D`

---

## 3. Estrutura do projeto e arquivos principais

| Caminho | Descrição |
| --- | --- |
| `samples/api/cco_tree_3d/cco_tree_3d.cpp` | Código principal. Parsing dos VTK, geração de geometria, animação e renderização. |
| `samples/api/cco_tree_3d/cco_tree_3d.h` | Estruturas de dados (segmentos, vértices, UBO) e parâmetros da UI. |
| `shaders/cco_tree_3d/glsl/tree3d.vert` | Vertex shader: aplica MVP, calcula normais e Gouraud. |
| `shaders/cco_tree_3d/glsl/tree3d.frag` | Fragment shader: iluminação Phong e destaque de seleção. |
| `assets/TP_CCO_Pacote_Dados/TP2_3D` | VTK 3D (Nterm 128, 256 e 512). |
| `docs/TP2_3D_DOCUMENTACAO.md` | Este documento. |

---

## 4. Arquitetura do fork Vulkan Samples

O fork mantém a arquitetura original do Vulkan Samples:
- **Framework**: classes base como `ApiVulkanSample`, gerenciamento de buffers, câmera e render.
- **Components**: utilitários (filesystem, logging, etc.).
- **App**: empacotamento para execução em desktop (GLFW) e Android.
- **Shaders**: GLSL compilados offline para SPIR-V.
- **Assets**: dados e recursos utilizados pela aplicação.

Cada amostra Vulkan herda `ApiVulkanSample`, com métodos fundamentais:
- `prepare()`
- `build_command_buffers()`
- `render()`
- `on_update_ui_overlay()`

---

## 5. Fluxo de execução da amostra `cco_tree_3d`

1. **Seleção do dataset via UI** (Nterm e `step`).
2. **Parsing do VTK**:
   - Leitura de `POINTS`, `LINES` e `CELL_DATA`.
   - Extração de conectividade e raios.
3. **Organização topológica**:
   - Grafo de adjacência com peso = comprimento.
   - Dijkstra calcula distância ao tronco.
   - `depth_factor = 1 - (dist / dist_max)`.
   - Ordenação final por `depth_factor` e `dataset_radius`.
4. **Geração da geometria**:
   - Cada segmento vira um cilindro triangulado.
   - Controle de lados (`sides`) e raio fixo/variável.
5. **Shaders**:
   - Vertex shader aplica MVP e iluminação Gouraud.
   - Fragment shader aplica Phong, coloração por raio e highlight da seleção.
6. **Transformações e câmera**:
   - Model matrix centraliza e escala a árvore.
   - Projeção perspectiva.
   - Câmera orbitante automática (ou manual pelo framework).
7. **Interface (ImGui)**:
   - Dataset, frame, animação.
   - Crescimento dos galhos.
   - Raio fixo/variável.
   - Iluminação (Gouraud/Phong).

---

## 6. Funcionalidades implementadas

### 6.1 Leitura e organização dos dados
- Compatível com todos os VTK 3D fornecidos.
- Extração automática de limites geométricos e variação de raios.
- Ordenação dos segmentos para crescimento incremental correto.

### 6.2 Geração de geometria 3D
- Segmentos convertidos em cilindros com tampas.
- Orientação via eixo do segmento + base ortonormal.
- Número de lados configurável.

### 6.3 Renderização 3D
- Pipeline Vulkan com **depth test/write** habilitado.
- Z-buffer ativado no render pass padrão.
- Blending desativado (alpha = 1.0).

### 6.4 Câmera e projeção
- Projeção **perspectiva** (FOV 60°, near 0.05, far 256).
- Câmera orbitante automática.
- Controle manual via `ApiVulkanSample`.

### 6.5 Iluminação (2 modelos)
- Gouraud no vertex shader.
- Phong no fragment shader.
- Alternância via UI.

### 6.6 Seleção de segmento
- Ray casting CPU com distância raio-segmento.
- Highlight no shader.
- UI exibe id, raio e comprimento do segmento.

### 6.7 Animação
- Troca de frames (`stepXXXX`) por tempo.
- Crescimento incremental por número de segmentos visíveis.

---

## 7. Detalhamento técnico adicional

### 7.1 Formato VTK e leitura
- **POINTS**: coordenadas 3D dos nós.
- **LINES**: pares de índices que formam segmentos.
- **CELL_DATA**: raio de cada segmento.
- A função `parse_vtk_file()` monta:
  - `points` (`glm::vec3`)
  - `segments` (pares de índices)
  - `radii` (float por segmento)

### 7.2 Organização topológica
- Grafo de adjacência com peso = comprimento do segmento.
- Dijkstra calcula distância ao tronco.
- `depth_factor` define ordem de crescimento.

### 7.3 Geração de cilindros
Para cada segmento:
1. `axis = normalize(b - a)`.
2. Base ortonormal (`tangent`, `bitangent`).
3. Anel de vértices em `sides`.
4. Índices para laterais e tampas.

### 7.4 UBO e matrizes
O UBO contém:
- `model`, `view`, `proj`, `mvp`
- `normal_matrix`
- `light_dir`, `camera_pos`, `radius_range`, `params`

### 7.5 Z-buffer
O depth buffer é criado pelo framework e anexado ao render pass.
Formato escolhido automaticamente entre:
- `VK_FORMAT_D32_SFLOAT`
- `VK_FORMAT_D24_UNORM_S8_UINT`
- `VK_FORMAT_D16_UNORM`

---

## 8. Processo de build e execução

### 8.1 macOS (desktop)
```bash
source /PATH/TO/VULKAN/SDK/setup-env.sh
cmake -B build/mac -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT=macosx -DCMAKE_OSX_DEPLOYMENT_TARGET=13.3
cmake --build build/mac --config Release --target vulkan_samples -j"$(sysctl -n hw.ncpu)"
./build/mac/app/bin/Release/$(uname -m)/vulkan_samples sample cco_tree_3d
```

### 8.2 Linux
```bash
cmake -G "Unix Makefiles" -Bbuild/linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux --config Release --target vulkan_samples -j$(nproc)
./build/linux/app/bin/Release/x86_64/vulkan_samples sample cco_tree_3d
```

### 8.3 Android
```bash
./scripts/generate.py android
```
Abra `build/android_gradle` no Android Studio e selecione o sample `cco_tree_3d`.

---

## 9. Links do vídeo e repositório

Link repositório:
```text
https://github.com/DoisDedin/Vulkan-Samples/tree/feat/test
```

Link vídeo:
```text
https://youtu.be/BUFrhXLMaLc?si=W-xBMDXfaBLEr8fJ
```

---

## 10. Referências

```text
https://github.com/KhronosGroup/Vulkan-Samples?tab=readme-ov-file
https://sempreupdate.com.br/qual-a-diferenca-entre-opengl-e-vulkan/
https://developer.android.com/games/develop/vulkan/overview?hl=pt-br
```

