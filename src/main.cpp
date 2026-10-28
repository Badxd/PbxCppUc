// cppgram userbot — connects via string session, no bot token needed.
// Env vars required: TG_API_ID, TG_API_HASH, TG_SESSION_STRING
// TG_SESSION_STRING = base64 string from the session generator bot
//
// Commands (type in any chat, they edit themselves):
//   .ping   .id   .time   .echo <text>   .alive

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <string>
#include <map>
#include <memory>

#include "cppgram/client.h"
#include "cppgram/mtproto/session.h"
#include "cppgram/session_string.h"

namespace {

const std::map<int, std::string> kDcAddresses = {
    {1, "149.154.175.53"}, {2, "149.154.167.51"}, {3, "149.154.175.100"},
    {4, "149.154.167.91"}, {5, "91.108.56.130"},
};

std::string env_or_throw(const char* name) {
    const char* v = std::getenv(name);
    if (!v || !*v) throw std::runtime_error(std::string("missing env: ") + name);
    return v;
}

std::string env_or(const char* name, const std::string& fallback = "") {
    const char* v = std::getenv(name);
    return (v && *v) ? std::string(v) : fallback;
}

std::string html_escape(const std::string& in) {
    std::string out;
    for (char c : in) {
        if      (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else out += c;
    }
    return out;
}

std::string args_of(const std::string& text) {
    const std::size_t space = text.find(' ');
    if (space == std::string::npos) return "";
    std::size_t start = space + 1;
    while (start < text.size() && text[start] == ' ') ++start;
    return text.substr(start);
}

std::string utc_now() {
    const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char buf[64];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S UTC", std::gmtime(&t));
    return buf;
}

} // namespace

int main() {
    try {
        const std::string api_id_str   = env_or_throw("TG_API_ID");
        const std::string api_hash     = env_or_throw("TG_API_HASH");
        const std::string session_str  = env_or_throw("TG_SESSION_STRING");
        const std::string session_path = env_or("TG_SESSION_PATH", "/tmp/userbot.sqlite");

        // Decode session to find which DC this account is on
        cppgram::mtproto::SessionData sd;
        if (!cppgram::decode_session_string(session_str, sd))
            throw std::runtime_error("invalid TG_SESSION_STRING — generate a new one");

        const int dc = sd.dc_id;
        const auto addr_it = kDcAddresses.find(dc);
        if (addr_it == kDcAddresses.end())
            throw std::runtime_error("unknown DC in session: " + std::to_string(dc));

        std::cout << "[+] session DC: " << dc << " (" << addr_it->second << ")\n";

        cppgram::ClientOptions opts;
        opts.api_id         = std::stoi(api_id_str);
        opts.api_hash       = api_hash;
        opts.dc_id          = dc;
        opts.dc_ip          = addr_it->second;
        opts.dc_port        = 443;
        opts.session_path   = session_path;
        opts.session_string = session_str; // restore auth — no re-login needed

        auto client = std::make_unique<cppgram::Client>(opts);

        client->on_error([](cppgram::Client&, const std::string& stage, const std::string& msg) {
            std::cerr << "[!] error in " << stage << ": " << msg << "\n";
        });

        client->connect();

        const auto me = client->get_me();
        std::cout << "[+] logged in as: " << me.first_name
                  << " (@" << me.username << ") id=" << me.id << "\n";

        // ── USERBOT COMMANDS (".prefix") ──────────────────────────────────────
        // All commands edit the sent message in-place (standard userbot UX).

        // .ping → Pong!
        client->on_message(cppgram::filters::outgoing() && cppgram::filters::command("ping", "."),
            [](cppgram::Client& c, const cppgram::Message& m) {
                c.edit_text(m.chat, m.id, "🏓 Pong!");
            });

        // .id → show chat id + your user id
        client->on_message(cppgram::filters::outgoing() && cppgram::filters::command("id", "."),
            [](cppgram::Client& c, const cppgram::Message& m) {
                const std::int64_t uid = m.sender ? m.sender->id : 0;
                c.edit_text(m.chat, m.id,
                    "📌 chat id: " + std::to_string(m.chat.id) +
                    "\n👤 user id: " + std::to_string(uid));
            });

        // .time → UTC timestamp
        client->on_message(cppgram::filters::outgoing() && cppgram::filters::command("time", "."),
            [](cppgram::Client& c, const cppgram::Message& m) {
                c.edit_text(m.chat, m.id, "🕐 " + utc_now());
            });

        // .echo <text> → edits message to just the text
        client->on_message(cppgram::filters::outgoing() && cppgram::filters::command("echo", "."),
            [](cppgram::Client& c, const cppgram::Message& m) {
                const std::string body = args_of(m.text);
                if (body.empty()) {
                    c.edit_text(m.chat, m.id, "Usage: .echo <text>");
                    return;
                }
                c.edit_text(m.chat, m.id, html_escape(body), cppgram::ParseMode::Html);
            });

        // .alive → confirms userbot is running
        client->on_message(cppgram::filters::outgoing() && cppgram::filters::command("alive", "."),
            [&me](cppgram::Client& c, const cppgram::Message& m) {
                c.edit_text(m.chat, m.id,
                    "<b>⚡ cppgram userbot alive</b>\n"
                    "👤 User: <b>" + html_escape(me.first_name) + "</b>\n"
                    "🕐 Time: " + utc_now(),
                    cppgram::ParseMode::Html);
            });

        std::cout << "[+] userbot running. Commands: .ping .id .time .echo .alive\n";
        client->run();

    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
