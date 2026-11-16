# AGENTS.md

## Regras permanentes do projeto

1. Este projeto é um fork da Godot 4.7.2 para desenvolvimento AI-first. Não é um plugin nem uma engine nova.
2. Preserve ao máximo o runtime, o editor 2D/3D, o Inspector, o sistema de animação e as ferramentas nativas da Godot.
3. Novas funcionalidades devem ficar isoladas principalmente em `editor/ai_workspace/`, com alterações mínimas nos arquivos existentes.
4. Antes de modificar código, inspecione as APIs reais da versão presente no repositório. Não invente classes, métodos ou caminhos.
5. Implemente somente o escopo explicitamente solicitado na etapa atual. Não antecipe etapas futuras.
6. Não instale servidores MCP de terceiros e não implemente um modelo de IA próprio. Agentes externos serão integrados por adaptadores específicos.
7. Preserve a compatibilidade dos projetos com a Godot oficial sempre que viável.
8. Use Undo/Redo para alterações de editor e não modifique projetos externos sem autorização.
9. Evite dependências externas desnecessárias. Não execute códigos não confiáveis da comunidade.
10. Implemente testes quando possível, execute a validação disponível e nunca afirme que uma compilação ou teste passou sem executá-lo.
11. Se uma ferramenta ou compilador estiver indisponível, documente o bloqueio e não simule resultados.
12. Ao concluir cada etapa, informe arquivos alterados, decisões técnicas, testes realizados, limitações e próximo passo. Faça um commit específico apenas se o repositório e ambiente permitirem.
13. Não remova arquivos, descarte alterações existentes ou execute ações destrutivas sem autorização.
14. Não implemente funcionalidades de etapas posteriores apenas para parecer mais completo.

