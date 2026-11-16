# Arquitetura inicial

Este projeto parte de um fork da Godot 4.7.2 voltado a um fluxo de desenvolvimento AI-first. O runtime, o editor 2D/3D, o Inspector, a animação e as ferramentas nativas permanecem como a base principal e devem receber o mínimo possível de alterações.

As funcionalidades específicas do workspace de IA serão concentradas em `editor/ai_workspace/`, funcionando como uma camada de integração do editor. Agentes externos serão conectados por adaptadores bem delimitados, sem incorporar um modelo de IA próprio nem exigir servidores MCP de terceiros. A camada deverá reutilizar as APIs reais da versão presente, respeitar Undo/Redo e manter a compatibilidade dos projetos com a Godot oficial sempre que viável.

Nesta etapa, o documento registra apenas os limites e as responsabilidades iniciais da arquitetura. Nenhuma implementação de código ou integração será iniciada.
