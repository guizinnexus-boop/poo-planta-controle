// Conversao de timestamps UTC no formato AAAA-MM-DDTHH:MM:SSZ.
#pragma once
#include <string>

namespace poo {

// Retorna false se o texto nao respeitar exatamente o formato ou for data impossivel.
bool analisar_utc(const std::string& texto, long long& epoch);
std::string formatar_utc(long long epoch);
long long agora_utc();

}  // namespace poo
