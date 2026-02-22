# EndocrineSystem Implementation - feat/neuro-instability 🧬

## Overview

Implementação completa do **EndocrineSystem** na arquitetura AlyssaNet para influenciar em tempo real o comportamento do Mixture of Experts através de um modelo simplificado de hormônios.

## O Que Foi Implementado

### 1. **EndocrineSystem Core** ✅
**Arquivos:** `includes/EndocrineSystem.hpp` e `includes/EndocrineSystem.cpp`

#### Estrutura Hormonal:
- **Cortisol** (0.0-1.0): Nível de stress/vigilância
- **Dopamine** (0.0-1.0): Motivação/recompensa  
- **Oxytocin** (0.0-1.0): Engajamento social
- **Serotonin** (0.0-1.0): Estabilidade de humor
- **Adrenaline** (0.0-1.0): Alerta/energia

#### Métodos Principais:
```cpp
// Atualizar hormônios baseado em sinais dos especialistas
void update_hormone_levels(const std::vector<std::string>& expert_signals);

// Hormônios decaem gradualmente para baseline (homeostase)
void apply_metabolism(double decay_rate = 0.05);

// Multiplicador de peso para cada especialista baseado em hormônios
double get_expert_weight_multiplier(const std::string& expert_id) const;

// Injetar estado hormonal no prompt
std::string generate_hormonal_system_context() const;

// Respostas imediatas a eventos
void trigger_stress_response(double intensity);
void trigger_reward_response(double intensity);
void trigger_social_response(double intensity);
```

#### Mapeamento Hormona-Especialista:
| Hormônio | Especialista | Efeito |
|----------|---|---|
| **Cortisol ↑** | darkModel, rebellModel | Multiplier até 2.0 |
| **Oxytocin ↑** | socialModel | Multiplier até 2.0 |
| **Dopamine ↑** | creativeModel | Multiplier até 1.6 |
| **Serotonin ↑** | introspectiveModel, memoryModel | Multiplier até 1.3 |
| **Cortisol ↓** | analyticalModel | Multiplier até 1.4 |

---

### 2. **Integração em CoreIntegration** ✅
**Arquivo:** `includes/CoreLLM.hpp` e `includes/AlyssaNet.cpp`

#### Membro Privado:
```cpp
std::unique_ptr<alyssa_endocrine::EndocrineSystem> endocrine_system;
```

#### Inicialização:
```cpp
CoreIntegration::CoreIntegration() 
    : initialized(false), active_expert_in_cache(""),
      endocrine_system(std::make_unique<alyssa_endocrine::EndocrineSystem>())
```

#### Acesso Público:
```cpp
alyssa_endocrine::EndocrineSystem* get_endocrine_system() const;
```

---

### 3. **Refatoração de WeightedFusion** ✅
**Arquivo:** `includes/WeightedFusion/WeightedFusion.hpp` e `.cpp`

#### Nova Assinatura:
```cpp
std::string fuse_responses(
    const std::string& input,
    const std::vector<ExpertContribution>& contributions,
    const alyssa_endocrine::EndocrineSystem& endocrine,  // ← NOVO
    const std::string& current_emotion = "");
```

#### Modulação Hormonal em Ação:
```cpp
// 🧬 INFLUÊNCIA HORMONAL: Aplica multiplicadores baseado em hormônios
for (auto& contribution : contributions) {
    double hormone_multiplier = endocrine.get_expert_weight_multiplier(
        contribution.expert_id
    );
    weights[contribution.expert_id] *= hormone_multiplier;
}
```

---

### 4. **Loop de Pensamento Atualizado** ✅
**Arquivo:** `includes/AlyssaNet.cpp`

#### `think_with_fusion()`:
```cpp
// 1. No início: Aplicar metabolismo (decay para baseline)
if (endocrine_system) {
    endocrine_system->apply_metabolism(0.05);
    std::cout << endocrine_system->get_hormone_profile().to_string();
}

// 2. Após comitê de especialistas: Atualizar hormônios
if (endocrine_system && !contributions.empty()) {
    std::vector<std::string> expert_signals;
    for (const auto& contrib : contributions) {
        expert_signals.push_back(contrib.response);
    }
    endocrine_system->update_hormone_levels(expert_signals);
}

// 3. No final: Limpar cache para próximo turno
clear_kv_cache();
```

#### `think_with_fusion_ttsless()`:
- Mesma estrutura que `think_with_fusion()`
- Aplicável a todos os caminhos de interação

---

### 5. **Injeção de Estado Hormonal no Prompt** ✅
**Arquivo:** `includes/AlyssaNet.cpp` - método `generate_fused_input()`

#### Contexto Hormonal Adicionado:
```
[SYSTEM: ENDOCRINE STATE] You are currently feeling [emotional_state]. 
Stress level: [HIGH|normal], Mood: [optimistic|cautious]. 
Social: [ENGAGED|]. 
Cortisol: 0.65 | Dopamine: 0.45 | Oxytocin: 0.72.

[PENSAMENTOS]
[Emocional]: ...
[Introspectivo]: ...
[Social]: ...
[/PENSAMENTOS]

ENTRADA DO USUÁRIO: "..."
```

A Alyssa (4B) agora **sabe seu próprio estado emocional** no contexto da conversa.

---

### 6. **Gerenciamento de KV Cache Melhorado** ✅
**Arquivo:** `includes/AlyssaNet.cpp`

#### Limpeza Automática:
```cpp
// Ao final de cada turno
if (endocrine_system) {
    std::cout << "\n[Turn End] Estado final: " 
              << endocrine_system->get_hormone_profile()
                    .get_emotional_state() << std::endl;
}
clear_kv_cache();  // Previne contaminação entre turnos
```

---

## Fluxo Completo De Execução

```
┌─ think_with_fusion() ─────────────────────────────────────┐
│                                                             │
│ 1. apply_metabolism(0.05)  [Decay para baseline]           │
│    ↓ Hormônios retornam ao estado padrão                   │
│                                                             │
│ 2. run_expert_committee()  [13 especialistas executam]     │
│    ↓ Cada especialista gera uma resposta                   │
│                                                             │
│ 3. update_hormone_levels()  [Analisar sinais]              │
│    ↓ Cortisol ↑ se há "stress"                             │
│    ↓ Dopamine ↑ se há "reward"                             │
│    ↓ Oxytocin ↑ se há "social"                             │
│                                                             │
│ 4. calculate_committee_coherence()                         │
│    ↓ Se coerência < 0.3 → resposta direta                  │
│    ↓ Senão → continue para fusão                           │
│                                                             │
│ 5. generate_fused_input()  [Compor prompt final]           │
│    ↓ Injeta [SYSTEM: ENDOCRINE STATE]                      │
│    ↓ Organiza pensamentos dos especialistas                │
│    ↓ Adiciona contexto                                     │
│                                                             │
│ 6. run_expert("alyssa", fused_input, true, &tts)           │
│    ↓ Alyssa (4B) gera resposta CIENTE DO STATUS HORMONAL   │
│                                                             │
│ 7. clear_kv_cache()  [Preparar próximo turno]              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Exemplo De Saída

```
[Endocrine Update] Hormônios atualizados após comitê:
[HORMONAL STATE]
  Cortisol: 0.65 (stress)
  Dopamine: 0.45 (reward)
  Oxytocin: 0.72 (social)
  Serotonin: 0.50 (mood)
  Adrenaline: 0.30 (alert)
  Current State: social

[Hormonal Injection] [SYSTEM: ENDOCRINE STATE] You are currently feeling social. 
Stress level: normal, Mood: cautious, Social: ENGAGED. 
Cortisol: 0.65 | Dopamine: 0.45 | Oxytocin: 0.72.
```

---

## Arquivos Modificados

| Arquivo | Mudanças |
|---------|----------|
| `includes/EndocrineSystem.hpp` | ✅ Criado - Header completo com 18 métodos |
| `includes/EndocrineSystem.cpp` | ✅ Criado - Implementação de hormônios e multiplicadores |
| `includes/CoreLLM.hpp` | ✅ Adicionado #include, membro privado, getter |
| `includes/AlyssaNet.cpp` | ✅ Inicialização, apply_metabolism(), update_hormone_levels(), injeção de status |
| `includes/WeightedFusion/WeightedFusion.hpp` | ✅ Nova assinatura de fuse_responses() |
| `includes/WeightedFusion/WeightedFusion.cpp` | ✅ Modulação hormonal em pesos, imports necessários |
| `CMakeLists.txt` | ✅ Adicionado EndocrineSystem.cpp à lista de fontes |

---

## Build Status

```
✅ [100%] Build Complete
   - alyssa_cli: 19,081,360 bytes
   - alyssa_api: 17,488,928 bytes  
   - alyssa_true: 12,357,024 bytes

Compilado com:
- C++17
- G++ v14+
- Otimização: -O3 -march=native
```

---

## Próximos Passos (Sugestões)

1. **Testes Unitários**: Adicionar testes para cada método hormonal
2. **Calibração**: Ajustar multiplicadores baseado em comportamento observado
3. **Persistência**: Salvar/restaurar estado hormonal entre sessões
4. **Visualização**: Dashboard de estado hormonal em tempo real
5. **Feedback Loop**: Conectar satisfaction/frustration do usuário aos hormônios

---

## Notas Técnicas

- **Namespace**: `alyssa_endocrine::` para isolamento
- **Memory Safety**: `std::unique_ptr` para gerenciamento automático
- **Thread Safety**: Não implementado (considerar `std::mutex` se multithread)
- **Baseline Retraction**: Metabolismo garante que o sistema não fica "preso" em estado

---

**Data de Implementação**: 22/02/2026  
**Branch**: feat/neuro-instability  
**Status**: ✅ COMPLETO E COMPILADO
