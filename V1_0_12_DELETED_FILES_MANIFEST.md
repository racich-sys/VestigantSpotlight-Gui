# V1.0.12 Deleted Files Manifest

No major source files were deleted in V1.0.12. Active versioned V1.0.11 convenience scripts were replaced by V1.0.12 equivalents.

Low-risk duplicated wrapper functions removed from app_runner.cpp:

- classifyIosAppDbRecordTable
- shouldUseWhatsappSpecialParser
- shouldUseAppleMessagesSpecialParser
- local buildKnowledgeCTextSnippet wrapper

The remaining Apple Messages, WhatsApp, KnowledgeC, and generic app DB row parser functions intentionally remain in app_runner.cpp until a parser-independent row sink is implemented.
