#include "car_rental/security.h"

#include "car_rental/errors.h"
#include "car_rental/utils.h"

#include <Poco/Base64Decoder.h>
#include <Poco/Base64Encoder.h>
#include <Poco/DigestEngine.h>
#include <Poco/HMACEngine.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/PBKDF2Engine.h>
#include <Poco/SHA2Engine.h>

#include <sstream>
#include <vector>

namespace car_rental {
namespace {

class Sha256CompatEngine : public Poco::SHA2Engine
{
public:
    enum
    {
        BLOCK_SIZE = 64,
        DIGEST_SIZE = 32
    };

    Sha256CompatEngine()
        : Poco::SHA2Engine(Poco::SHA2Engine::ALGORITHM::SHA_256)
    {
    }
};

using HmacSha256 = Poco::HMACEngine<Sha256CompatEngine>;
using PasswordDerivation = Poco::PBKDF2Engine<HmacSha256>;

std::string stringify(const Poco::JSON::Object::Ptr& object)
{
    std::ostringstream output;
    Poco::JSON::Stringifier::stringify(object, output);
    return output.str();
}

std::string base64UrlEncode(const std::string& raw)
{
    std::ostringstream encoded;
    Poco::Base64Encoder encoder(encoded);
    encoder.rdbuf()->setLineLength(0);
    encoder.write(raw.data(), static_cast<std::streamsize>(raw.size()));
    encoder.close();

    std::string value = encoded.str();
    while (!value.empty() && value.back() == '=')
        value.pop_back();

    for (char& ch : value)
    {
        if (ch == '+')
            ch = '-';
        else if (ch == '/')
            ch = '_';
    }
    return value;
}

std::string base64UrlDecode(std::string encoded)
{
    for (char& ch : encoded)
    {
        if (ch == '-')
            ch = '+';
        else if (ch == '_')
            ch = '/';
    }

    while (encoded.size() % 4 != 0)
        encoded.push_back('=');

    std::istringstream input(encoded);
    Poco::Base64Decoder decoder(input);
    std::ostringstream output;
    output << decoder.rdbuf();
    return output.str();
}

std::string signHmac(const std::string& value, const std::string& secret)
{
    HmacSha256 engine(secret);
    engine.update(value);
    const auto& digest = engine.digest();
    return std::string(reinterpret_cast<const char*>(digest.data()), digest.size());
}

bool constantTimeEquals(const std::string& left, const std::string& right)
{
    if (left.size() != right.size())
        return false;

    unsigned char diff = 0;
    for (std::size_t i = 0; i < left.size(); ++i)
        diff |= static_cast<unsigned char>(left[i] ^ right[i]);
    return diff == 0;
}

std::int64_t unixNow()
{
    return Poco::Timestamp{}.epochTime();
}

std::int64_t unixAfter(long ttlSeconds)
{
    return unixNow() + ttlSeconds;
}

} // namespace

std::string PasswordHasher::generateSalt() const
{
    return generateUuid();
}

std::string PasswordHasher::hashPassword(const std::string& password, const std::string& salt) const
{
    PasswordDerivation pbkdf2(salt, 6000, 32);
    pbkdf2.update(password);
    return Poco::DigestEngine::digestToHex(pbkdf2.digest());
}

bool PasswordHasher::verify(const std::string& password, const std::string& salt, const std::string& expectedHash) const
{
    return hashPassword(password, salt) == expectedHash;
}

JwtService::JwtService(std::string secret, long ttlSeconds)
    : secret_(std::move(secret))
    , ttlSeconds_(ttlSeconds)
{
}

std::string JwtService::issueToken(const AuthenticatedUser& user) const
{
    Poco::JSON::Object::Ptr header = new Poco::JSON::Object;
    header->set("alg", "HS256");
    header->set("typ", "JWT");

    Poco::JSON::Object::Ptr payload = new Poco::JSON::Object;
    payload->set("sub", user.id);
    payload->set("login", user.login);
    payload->set("role", toString(user.role));
    payload->set("exp", unixAfter(ttlSeconds_));

    const std::string headerPart = base64UrlEncode(stringify(header));
    const std::string payloadPart = base64UrlEncode(stringify(payload));
    const std::string signingInput = headerPart + "." + payloadPart;
    const std::string signature = base64UrlEncode(signHmac(signingInput, secret_));

    return signingInput + "." + signature;
}

TokenPayload JwtService::verifyToken(const std::string& token) const
{
    const auto firstDot = token.find('.');
    const auto secondDot = token.find('.', firstDot == std::string::npos ? firstDot : firstDot + 1);
    if (firstDot == std::string::npos || secondDot == std::string::npos)
        throw ApiException(401, "unauthorized", "Invalid token format.");

    const std::string headerPart = token.substr(0, firstDot);
    const std::string payloadPart = token.substr(firstDot + 1, secondDot - firstDot - 1);
    const std::string signaturePart = token.substr(secondDot + 1);
    const std::string signingInput = headerPart + "." + payloadPart;

    const std::string expectedSignature = signHmac(signingInput, secret_);
    const std::string providedSignature = base64UrlDecode(signaturePart);
    if (!constantTimeEquals(expectedSignature, providedSignature))
        throw ApiException(401, "unauthorized", "Token signature verification failed.");

    Poco::JSON::Parser parser;
    auto headerVar = parser.parse(base64UrlDecode(headerPart));
    auto payloadVar = parser.parse(base64UrlDecode(payloadPart));
    auto header = headerVar.extract<Poco::JSON::Object::Ptr>();
    auto payload = payloadVar.extract<Poco::JSON::Object::Ptr>();

    if (header->getValue<std::string>("alg") != "HS256")
        throw ApiException(401, "unauthorized", "Unsupported token algorithm.");

    TokenPayload tokenPayload;
    tokenPayload.subject = payload->getValue<std::string>("sub");
    tokenPayload.login = payload->getValue<std::string>("login");
    tokenPayload.role = roleFromString(payload->getValue<std::string>("role"));
    tokenPayload.expiresAtUnix = payload->getValue<std::int64_t>("exp");

    if (tokenPayload.expiresAtUnix <= unixNow())
        throw ApiException(401, "unauthorized", "Token has expired.");

    return tokenPayload;
}

long JwtService::ttlSeconds() const noexcept
{
    return ttlSeconds_;
}

} // namespace car_rental
