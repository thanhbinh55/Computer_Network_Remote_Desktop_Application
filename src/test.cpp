#include "modules/AppManager.hpp"

int main() {
    AppManager am;
    json request = {{"command", "START"}, {"payload", {{"path", "firefox"}}}};
    am.handle_command(request);
    return 0;
}
