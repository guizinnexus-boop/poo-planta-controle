#include "tempo.hpp"

#include <cstdio>
#include <ctime>

namespace poo {
namespace {

// Algoritmos de data civil (Howard Hinnant), validos para o calendario gregoriano.
long long dias_desde_epoca(long long y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long long>(doe) - 719468;
}

void data_civil(long long z, long long& y, unsigned& m, unsigned& d) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = static_cast<long long>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp + (mp < 10 ? 3 : -9);
    y += (m <= 2);
}

bool bissexto(long long y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

unsigned dias_no_mes(long long y, unsigned m) {
    static const unsigned d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2 && bissexto(y)) ? 29 : d[m - 1];
}

bool digitos(const std::string& s, size_t ini, size_t n, int& out) {
    int v = 0;
    for (size_t k = ini; k < ini + n; ++k) {
        if (s[k] < '0' || s[k] > '9') return false;
        v = v * 10 + (s[k] - '0');
    }
    out = v;
    return true;
}

}  // namespace

bool analisar_utc(const std::string& s, long long& epoch) {
    if (s.size() != 20) return false;
    if (s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' || s[16] != ':' || s[19] != 'Z') return false;
    int Y, M, D, h, mi, se;
    if (!digitos(s, 0, 4, Y) || !digitos(s, 5, 2, M) || !digitos(s, 8, 2, D) ||
        !digitos(s, 11, 2, h) || !digitos(s, 14, 2, mi) || !digitos(s, 17, 2, se))
        return false;
    if (M < 1 || M > 12) return false;
    if (D < 1 || static_cast<unsigned>(D) > dias_no_mes(Y, static_cast<unsigned>(M))) return false;
    if (h > 23 || mi > 59 || se > 59) return false;
    epoch = dias_desde_epoca(Y, static_cast<unsigned>(M), static_cast<unsigned>(D)) * 86400LL + h * 3600LL + mi * 60LL + se;
    return true;
}

std::string formatar_utc(long long epoch) {
    long long dias = epoch >= 0 ? epoch / 86400 : -((-epoch + 86399) / 86400);
    long long resto = epoch - dias * 86400;
    long long y;
    unsigned m, d;
    data_civil(dias, y, m, d);
    char buf[96];
    std::snprintf(buf, sizeof buf, "%04lld-%02u-%02uT%02lld:%02lld:%02lldZ", y, m, d, resto / 3600, (resto / 60) % 60, resto % 60);
    return buf;
}

long long agora_utc() { return static_cast<long long>(std::time(nullptr)); }

}  // namespace poo
