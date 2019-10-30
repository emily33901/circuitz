#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui.h"
#include "nodes/ImNodesEz.h"
#include <map>
#include <string>
#include <vector>

// A structure defining a connection between two slots of two nodes.
struct Connection {
    // `id` that was passed to BeginNode() of input node.
    void *input_node = nullptr;
    // Descriptor of input slot.
    const char *input_slot = nullptr;
    // `id` that was passed to BeginNode() of output node.
    void *output_node = nullptr;
    // Descriptor of output slot.
    const char *output_slot = nullptr;

    bool operator==(const Connection &other) const {
        return input_node == other.input_node &&
               input_slot == other.input_slot &&
               output_node == other.output_node &&
               output_slot == other.output_slot;
    }

    bool operator!=(const Connection &other) const {
        return !operator==(other);
    }
};

enum NodeSlotTypes {
    NodeSlotPower = 1,
};

enum class PowerState {
    off,
    on,
    broken,
};

// A structure holding node state.
class Node {
public:
    // Title which will be displayed at the center-top of the node.
    const char *title = nullptr;
    // Flag indicating that node is selected by the user.
    bool selected = false;
    // Node position on the canvas.
    ImVec2 pos{};
    // List of node connections.
    std::vector<Connection> connections{};
    // A list of input slots current node has.
    std::vector<ImNodes::Ez::SlotInfo> input_slots{};
    // A list of output slots current node has.
    std::vector<ImNodes::Ez::SlotInfo> output_slots{};

    // RenderUI is called during rendering to allow this node to render
    // its options
    virtual void RenderUI() = 0;

    // State returns the current state of the node by calculating the state
    // from the nodes before it
    virtual PowerState State() = 0;

    explicit Node(const char *                               title,
                  const std::vector<ImNodes::Ez::SlotInfo> &&input_slots,
                  const std::vector<ImNodes::Ez::SlotInfo> &&output_slots) {
        this->title        = title;
        this->input_slots  = input_slots;
        this->output_slots = output_slots;
    }

    virtual ~Node() = default;

    // Deletes connection from this node.
    void DeleteConnection(const Connection &connection) {
        for (auto it = connections.begin(); it != connections.end(); ++it) {
            if (connection == *it) {
                connections.erase(it);
                break;
            }
        }
    }
};

const char inputLabels[] = "A\0B\0C\0D\0E\0F\0G\0H\0I\0J\0K\0L\0M\0N\0O\0P\0Q\0R\0S\0T\0U\0V\0W\0X\0Y\0Z\0";

// SimpleNode can model most nodes
// It can have arbitrarily many inputs and only one output
class SimpleNode : public Node {
public:
    int numberInputs  = 2;
    int defaultInputs = 2;

    SimpleNode(const char *title, int defaultInputCount = 2)
        : Node(title, {}, {{"Out", NodeSlotPower}}) {
        defaultInputs = defaultInputCount;
        numberInputs  = defaultInputCount;

        for (int i = 0; i < numberInputs; i++) {
            input_slots.push_back({&inputLabels[i * 2], NodeSlotPower});
        }
    }

    void RenderUI() override {
        if (ImGui::InputInt("Inputs", &numberInputs)) {
            if (numberInputs > 26) {
                numberInputs = 26;
            } else if (numberInputs < defaultInputs) {
                numberInputs = defaultInputs;
            }

            if (input_slots.size() > numberInputs) {
                // Get rid of existing connections from these slots

                for (int i = input_slots.size() - 1; i >= numberInputs; i--) {
                    for (auto &c : connections) {
                        if (strcmp(c.input_slot, input_slots[i].title) == 0) {
                            ((Node *)c.output_node)->DeleteConnection(c);
                            ((Node *)c.input_node)->DeleteConnection(c);
                        }
                    }
                }

                input_slots.resize(numberInputs);
            } else if (input_slots.size() < numberInputs) {
                // append more input slots
                for (int i = input_slots.size(); i < numberInputs; i++) {
                    input_slots.push_back({&inputLabels[i * 2], NodeSlotPower});
                }
            }
        }

        ImGui::Text("State is %d", State());
    }
};

#define DefineBasicNode(name, inputCount, onBlock, offBlock, resultTransformBlock) \
    class name##Node : public SimpleNode {                                         \
    public:                                                                        \
        explicit name##Node()                                                      \
            : SimpleNode(#name, inputCount) {                                      \
        }                                                                          \
                                                                                   \
        PowerState State() override {                                              \
            int  state        = false;                                             \
            auto nodesCovered = 0;                                                 \
                                                                                   \
            for (auto &x : connections) {                                          \
                if (x.input_node == this) {                                        \
                                                                                   \
                    /* Keep track of how many inputs we have enumerated            \
                    if we havent done all of them then we are broken*/             \
                    ++nodesCovered;                                                \
                                                                                   \
                    switch (((Node *)x.output_node)->State()) {                    \
                    case PowerState::on:                                           \
                        if (nodesCovered == 1)                                     \
                            state = 1;                                             \
                        else                                                       \
                            do {                                                   \
                                onBlock;                                           \
                            } while (0);                                           \
                        break;                                                     \
                    case PowerState::off:                                          \
                        if (nodesCovered == 1)                                     \
                            state = 0;                                             \
                        else                                                       \
                            do {                                                   \
                                offBlock;                                          \
                            } while (0);                                           \
                        break;                                                     \
                    case PowerState::broken:                                       \
                        /* Once we know that this circuit is broken                \
                         then we can just return*/                                 \
                        return PowerState::broken;                                 \
                    }                                                              \
                }                                                                  \
            }                                                                      \
                                                                                   \
            if (nodesCovered != numberInputs) return PowerState::broken;           \
            do {                                                                   \
                resultTransformBlock;                                              \
            } while (0);                                                           \
            return state ? PowerState::on : PowerState::off;                       \
        }                                                                          \
    };

DefineBasicNode(Or, 2, state = 1, state = state || 0, {});
DefineBasicNode(Nor, 2, state = 1, state = state || 0, state = !state);
DefineBasicNode(And, 2, state = state && 1, state = false, {});
DefineBasicNode(Nand, 2, state = state && 1, state = false, state = !state);
DefineBasicNode(Xor, 2, state += 1, {}, state = (state % 2 == 1));
DefineBasicNode(Xnor, 2, state += 1, {}, state = (state % 2 == 0));
DefineBasicNode(Not, 1, state = true, state = false, state = !state);

class SwitchNode : public Node {
public:
    bool powered = false;

    explicit SwitchNode()
        : Node("Switch", {}, {{"Power", NodeSlotPower}}) {
    }

    PowerState State() override {
        return powered ? PowerState::on : PowerState::off;
    }

    void RenderUI() override {
        ImGui::Checkbox("Powered", &powered);
    }
};

#include <chrono>

class ClockNode : public Node {
public:
    int cycle = 1000;

    std::chrono::time_point<std::chrono::system_clock> last_cycle;

    explicit ClockNode()
        : Node("Clock", {}, {{"Power", NodeSlotPower}}) {
    }

    PowerState State() override {
        auto now = std::chrono::system_clock::now();

        auto diff = now - last_cycle;

        if ((std::chrono::duration_cast<std::chrono::milliseconds>(diff) / (std::chrono::milliseconds(cycle))) % 2 == 0) {
            return PowerState::on;
        } else {
            return PowerState::off;
        }
    }

    void RenderUI() override {
        if (ImGui::DragInt("", &cycle, 1, 0, 0, "%f ms") || ImGui::Button("Reset cycle")) {

            if (cycle <= 0) cycle = 1;

            last_cycle = std::chrono::system_clock::now();
        }
    }
};

class LightNode : public Node {
public:
    explicit LightNode()
        : Node("Light", {{"Power", NodeSlotPower}}, {}) {
    }

    PowerState State() override {
        for (auto &x : connections) {
            return ((Node *)x.output_node)->State();
        }
        return PowerState::broken;
    }

    void RenderUI() override {
    }
};

class TestNode : public Node {
public:
    explicit TestNode()
        : Node("Test", {{"1", NodeSlotPower}, {"2", NodeSlotPower}, {"3", NodeSlotPower}, {"4", NodeSlotPower}, {"5", NodeSlotPower}},
               {{"1", NodeSlotPower}, {"2", NodeSlotPower}, {"3", NodeSlotPower}, {"4", NodeSlotPower}, {"5", NodeSlotPower}}) {
    }

    PowerState State() override {
        return PowerState::broken;
    }

    void RenderUI() override {
        ImGui::Text("This is a test node!");
    }
};

std::map<std::string, Node *(*)()> transform_nodes{
    {"Or", []() -> Node * {
         return new OrNode();
     }},
    {"Nor", []() -> Node * {
         return new NorNode();
     }},
    {"And", []() -> Node * {
         return new AndNode();
     }},
    {"Nand", []() -> Node * {
         return new NandNode();
     }},
    {"Xor", []() -> Node * {
         return new XorNode();
     }},
    {"Xnor", []() -> Node * {
         return new XnorNode();
     }},
    {"Not", []() -> Node * {
         return new NotNode();
     }},
    {"Test", []() -> Node * {
         return new TestNode();
     }},
};

std::map<std::string, Node *(*)()> source_nodes{
    {"Switch", []() -> Node * {
         return new SwitchNode();
     }},
    {"Clock", []() -> Node * {
         return new ClockNode();
     }},
};

std::map<std::string, Node *(*)()> end_nodes{
    {"Light", []() -> Node * {
         return new LightNode();
     }},
};

std::vector<Node *> nodes;

ImU32 stateColors[] = {
    IM_COL32(200, 200, 200, 255),
    IM_COL32(0, 255, 0, 255),
    IM_COL32(255, 0, 0, 255),
};

bool CustomBeginNode(Node *n) {
    bool result = ImNodes::BeginNode(n, &n->pos, &n->selected);

    auto state = n->State();

    ImVec2 title_size    = ImGui::CalcTextSize(n->title);
    auto   circle_radius = title_size.y * 0.5f;
    auto   original_x    = ImGui::GetCursorPosX();

    auto *storage    = ImGui::GetStateStorage();
    float node_width = storage->GetFloat(ImGui::GetID("node-width"));

    // Center powered on
    if (node_width > 0)
        if (node_width > title_size.x)
            ImGui::SetCursorPosX(original_x + node_width / 2.f - (title_size.x) / 2.f - ImGui::GetStyle().ItemSpacing.x - circle_radius);

    ImRect circle_rect{
        ImGui::GetCursorScreenPos(),
        ImGui::GetCursorScreenPos() + ImVec2{circle_radius, circle_radius}};
    // Vertical-align circle in the middle of the line.
    float circle_offset_y = title_size.y / 2.f - circle_radius / 2;
    circle_rect.Min.y += circle_offset_y;
    circle_rect.Max.y += circle_offset_y;

    ImGui::GetWindowDrawList()->AddCircleFilled(circle_rect.GetCenter(), circle_radius, stateColors[(int)state]);

    // Render node title
    if (node_width > 0 && node_width > title_size.x)
        ImGui::SetCursorPosX(original_x + node_width / 2.f - (title_size.x) / 2.f);

    ImGui::TextUnformatted(n->title);

    ImGui::BeginGroup();
    return result;
}

void circuitz() {
    // Canvas must be created after ImGui initializes, because constructor accesses ImGui style to configure default colors.
    static ImNodes::CanvasState canvas{};

    const ImGuiStyle &style = ImGui::GetStyle();
    if (ImGui::Begin("ImNodes", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        // We probably need to keep some state, like positions of nodes/slots for rendering connections.
        ImNodes::BeginCanvas(&canvas);
        for (auto it = nodes.begin(); it != nodes.end();) {
            Node *node = *it;

            // Start rendering node
            if (CustomBeginNode(node)) {
                // Render input nodes first (order is important)
                ImNodes::Ez::InputSlots(node->input_slots.data(), node->input_slots.size());

                // Custom node content may go here
                ImGui::PushItemWidth(160);
                node->RenderUI();
                ImGui::PopItemWidth();

                // Render output nodes first (order is important)
                ImNodes::Ez::OutputSlots(node->output_slots.data(), node->output_slots.size());

                // Store new connections when they are created
                Connection new_connection;
                if (ImNodes::GetNewConnection(&new_connection.input_node, &new_connection.input_slot,
                                              &new_connection.output_node, &new_connection.output_slot)) {
                    ((Node *)new_connection.input_node)->connections.push_back(new_connection);
                    ((Node *)new_connection.output_node)->connections.push_back(new_connection);
                }

                // Render output connections of this node
                for (const Connection &connection : node->connections) {
                    // Node contains all it's connections (both from output and to input slots). This means that multiple
                    // nodes will have same connection. We render only output connections and ensure that each connection
                    // will be rendered once.
                    if (connection.output_node != node)
                        continue;

                    if (!ImNodes::Connection(connection.input_node, connection.input_slot, connection.output_node,
                                             connection.output_slot, &stateColors[(int)node->State()])) {
                        // Remove deleted connections
                        ((Node *)connection.input_node)->DeleteConnection(connection);
                        ((Node *)connection.output_node)->DeleteConnection(connection);
                    }
                }
            }
            // Node rendering is done. This call will render node background based on size of content inside node.
            ImNodes::Ez::EndNode();

            if (node->selected && ImGui::IsKeyPressedMap(ImGuiKey_Delete)) {
                // Deletion order is critical: first we delete connections to us
                for (auto &connection : node->connections) {
                    if (connection.output_node == node) {
                        ((Node *)connection.input_node)->DeleteConnection(connection);
                    } else {
                        ((Node *)connection.output_node)->DeleteConnection(connection);
                    }
                }
                // Then we delete our own connections, so we don't corrupt the list
                node->connections.clear();

                delete node;
                it = nodes.erase(it);
            } else
                ++it;
        }

        const ImGuiIO &io = ImGui::GetIO();
        if (ImGui::IsMouseReleased(1) && ImGui::IsWindowHovered() && !ImGui::IsMouseDragging(1)) {
            ImGui::FocusWindow(ImGui::GetCurrentWindow());
            ImGui::OpenPopup("NodesContextMenu");
        }

        if (ImGui::BeginPopup("NodesContextMenu")) {
            for (const auto &desc : source_nodes) {
                if (ImGui::MenuItem(desc.first.c_str())) {
                    nodes.push_back(desc.second());
                    ImNodes::AutoPositionNode(nodes.back());
                }
            }
            ImGui::Separator();
            for (const auto &desc : end_nodes) {
                if (ImGui::MenuItem(desc.first.c_str())) {
                    nodes.push_back(desc.second());
                    ImNodes::AutoPositionNode(nodes.back());
                }
            }
            ImGui::Separator();
            for (const auto &desc : transform_nodes) {
                if (ImGui::MenuItem(desc.first.c_str())) {
                    nodes.push_back(desc.second());
                    ImNodes::AutoPositionNode(nodes.back());
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Zoom"))
                canvas.zoom = 1;

            if (ImGui::IsAnyMouseDown() && !ImGui::IsWindowHovered())
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImNodes::EndCanvas();
    }
    ImGui::End();
}