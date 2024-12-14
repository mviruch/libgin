#include "router.h"
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

enum NodeType
{
    Static,
    Root,
    Param,
};

struct node
{
    std::string path;
    std::string indices;
    NodeType type;
    bool wildChild;
    std::vector<node *> children;
    HandlerChain handler;

    void addRoute(std::string path, HandlerChain handler);
    void insertChild(std::string path, std::string fullPath,
                     HandlerChain handler);
    // Recursively finds the node value or returns trailing slash recommendation
    auto getValue(std::string path, Params *params, bool &tsr) -> HandlerChain;
};

// Helper function to find the longest common prefix
auto longestCommonPrefix(const std::string &a, const std::string &b) -> int
{
    int i = 0;
    for (; i < a.size() && i < b.size() && a[i] == b[i]; i++)
        ;
    return i;
}

// Function to find wildcard in path, such as :param or *catchall
auto findWildcard(const std::string &path, std::string &wildcard,
                  int &pos) -> bool
{
    for (int i = 0; i < path.size(); ++i)
    {
        char c = path[i];
        if (c != ':')
            continue; // Ignore anything other than ":"

        bool valid = true;
        for (int j = i + 1; j < path.size(); ++j)
        {
            if (path[j] == '/')
            {
                wildcard = path.substr(i, j - i);
                pos = i;
                return valid;
            }
            else if (path[j] == ':' || path[j] == '*')
            {
                valid = false;
            }
        }

        wildcard = path.substr(i);
        pos = i;
        return valid;
    }

    pos = -1;
    return false;
}

auto cleanPath(const std::string &p) -> std::string
{
    const size_t stackBufSize = 128;
    std::vector<char> buf(stackBufSize);

    if (p.size() == 0)
    {
        return "/";
    }

    size_t n = p.size();

    size_t r = 1, w = 1;

    // 将p的内容复制到buf中
    for (int i = 0; i < std::min(stackBufSize, n); i++)
    {
        buf[i] = p[i];
    }

    if (p[0] != '/')
    {
        r = 0;
        buf.resize(n + 1);
        buf[0] = '/';
    }

    bool trailing = n > 1 && p[n - 1] == '/';

    for (; r < n; r++)
    {
        if (p[r] == '/')
        {
            r++;
        }
        else if (p[r] == '.' && r + 1 == n)
        {
            trailing = true;
            r++;
        }
        else if (p[r] == '.' && p[r + 1] == '/')
        {
            if (r + 2 == n || p[r + 2] == '/')
            {
                r += 3;
                if (w > 1)
                {
                    w--;

                    if (buf.size() == 0)
                    {
                        for (; w > 1 && p[w] != '/';)
                            w--;
                    }
                    else
                    {
                        for (; w > 1 && buf[w] != '/';)
                            w--;
                    }
                }
            }
            else
            {
                r += 2;
            }
        }
        else
        {
            if (w > 1)
            {
                buf[w] = '/';
                w++;
            }

            for (; r < n && p[r] != '/'; r++, w++)
            {
                buf[w] = p[r];
            }
        }
    }

    if (trailing && w > 1)
    {
        buf[w] = '/';
        w++;
    }

    if (buf.size() == 0)
    {
        return {buf.begin(), buf.begin() + w};
    }

    return {buf.begin(), buf.begin() + w};
}

auto RouterGroup::group(std::string relativePath) -> RouterGroup *
{
    return new RouterGroup(calculateAbsolutePath(relativePath), handlers,
                           engine);
}

void RouterGroup::handle(const std::string &method, const std::string &path,
                         RouteHandler handler)
{
    if (engine->trees.find(method) == engine->trees.end())
    {
        engine->trees[method] = new node();
    }
    node *root = engine->trees[method];
    // TODO: Add route to the tree
    root->addRoute(calculateAbsolutePath(path),
                   combineHandlers([handler](Context *ctx)
                                   { handler(ctx->getRequest(), ctx->getResponse(), ctx); }));
}

void RouterGroup::use(Handler handler) { handlers.push_back(handler); }

auto RouterGroup::calculateAbsolutePath(const std::string &relativePath)
    -> std::string
{
    return basePath + relativePath;
}

auto RouterGroup::combineHandlers(Handler handler) -> HandlerChain
{
    // 将当前的handlers复制, 然后加上engine的handlers
    HandlerChain combined = handlers;
    combined.push_back(handler);
    return combined;
}

// void Engine::ServeHTTP(const std::string &method, const std::string &path)
// {
//     if (trees.find(method) == trees.end())
//     {
//         throw std::runtime_error("Method " + method + " not allowed");
//     }

//     auto cleanedPath = cleanPath(path);

//     node *root = trees[method];
//     Params params;
//     bool tsr = false;
//     HandlerChain handler = root->getValue(cleanedPath, &params, tsr);

//     if (handler.size() > 0)
//     {
//         Context ctx(method, path, params, handler);
//         ctx.next();
//     }
//     else if (tsr)
//     {
//         printf("Redirect to: %s/\n", path.c_str());
//     }
//     else
//     {
//         printf("404 Not Found: %s\n", path.c_str());
//     }
// }

void Engine::ServeHTTP(request_s &req, response_s &res)
{
    auto method = req.method;
    auto path = req.url;

    if (trees.find(method) == trees.end())
    {
        throw std::runtime_error("Method " + method + " not allowed");
    }

    auto cleanedPath = cleanPath(path);

redirect:
    node *root = trees[method];
    Params params;
    bool tsr = false;
    HandlerChain handler = root->getValue(cleanedPath, &params, tsr);

    if (handler.size() > 0)
    {
        Context ctx(&req, &res, params, handler);
        ctx.next();
    }
    else if (tsr)
    {
        // printf("Redirect to: %s/\n", path.c_str());
        cleanedPath += "/";
        goto redirect;
    }
    else
    {
        // printf("404 Not Found: %s\n", path.c_str());
        if (noroute != nullptr)
        {
            Context ctx(&req, &res, params, {noroute});
            ctx.next();
        }
        else
        {
            res.setStatus(404)
                ->addHeader("Content-Type", "text/plain")
                ->setBody("404 Not Found");
        }
    }
}

// Method to walk through the tree and find the handle or trailing slash
// recommendation
auto node::getValue(std::string path, Params *params,
                    bool &tsr) -> HandlerChain
{
    auto n = this;
    std::string prefix;

walk_tree:
    while (true)
    {
        prefix = n->path;

        if (path.size() > prefix.size())
        {
            // Path starts with current node's prefix
            if (path.substr(0, prefix.size()) == prefix)
            {
                path = path.substr(prefix.size());

                // No wildcard child
                if (!n->wildChild)
                {
                    char idxc = path[0];
                    for (size_t i = 0; i < n->indices.size(); ++i)
                    {
                        if (n->indices[i] == idxc)
                        {
                            n = n->children[i];
                            goto walk_tree;
                        }
                    }

                    // No match found, check for trailing slash recommendation
                    tsr = (path == "/" && n->handler.size() > 0);
                    return {};
                }

                // Handle wildcard child
                n = n->children[0];
                switch (n->type)
                {
                case Param:
                {
                    size_t end = 0;
                    while (end < path.size() && path[end] != '/')
                    {
                        end++;
                    }

                    // Save param value into unordered_map
                    if (params)
                    {
                        params->emplace(n->path.substr(1), path.substr(0, end));
                    }

                    if (end < path.size())
                    {
                        if (!n->children.empty())
                        {
                            path = path.substr(end);
                            n = n->children[0];
                            goto walk_tree;
                        }

                        // No deeper path to follow, recommend TSR
                        tsr = (path.size() == end + 1);
                        return {};
                    }

                    if (n->handler.size() > 0)
                    {
                        return n->handler;
                    }
                    else if (n->children.size() == 1)
                    {
                        n = n->children[0];
                        tsr = (n->path == "/" && n->handler.size() > 0) ||
                              (n->path.empty() && n->indices == "/");
                    }
                    return {};
                }

                    // case CatchAll:
                    //     // Save param value into unordered_map for catch-all
                    //     if (params) {
                    //         params->emplace(n->path.substr(2), path);
                    //     }
                    //     return n->handle;

                default:
                    throw std::runtime_error("Invalid node type");
                }
            }
        }
        else if (path == prefix)
        {
            // If the path exactly matches this node
            if (n->handler.size() > 0)
            {
                return n->handler;
            }

            // No handle for this route, check for wildcard child
            if (path == "/" && n->wildChild && n->type != Root)
            {
                tsr = true;
                return {};
            }

            if (path == "/" && n->type == Static)
            {
                tsr = true;
                return {};
            }

            // Check for trailing slash by looking through indices
            for (size_t i = 0; i < n->indices.size(); ++i)
            {
                if (n->indices[i] == '/')
                {
                    n = n->children[i];
                    tsr = (n->path.size() == 1 && n->handler.size() > 0) ||
                          (n->children[0]->handler.size() > 0);
                    return {};
                }
            }
            return {};
        }

        // No match, check for trailing slash redirection
        tsr = (path == "/" || (prefix.size() == path.size() + 1 &&
                               prefix[path.size()] == '/' &&
                               path == prefix.substr(0, prefix.size() - 1) &&
                               n->handler.size() > 0));
        return {};
    }
}

void node::addRoute(std::string path, HandlerChain handler)
{
    node *n = this;
    std::string fullPath = path;

    // Case 1: Empty tree, root initialization
    if (n->path.empty() && n->indices.empty())
    {
        n->insertChild(path, fullPath, handler);
        type = Root;
        return;
    }

    // Iterate through the path segments
    while (true)
    {
        int commonPrefixLen = longestCommonPrefix(path, n->path);

        // Case 2: Split node if necessary
        if (commonPrefixLen < n->path.size())
        {
            node *child = new node{n->path.substr(commonPrefixLen),
                                   n->indices,
                                   Static,
                                   n->wildChild,
                                   n->children,
                                   n->handler};
            n->children = {child};
            n->indices = n->path[commonPrefixLen];
            n->path = n->path.substr(0, commonPrefixLen);
            n->wildChild = false;
            n->handler = {};
        }

        // Case 3: Path and node have common prefix, continue matching
        if (commonPrefixLen < path.size())
        {
            path = path.substr(commonPrefixLen);

            if (n->wildChild)
            {
                if (path.size() >= n->path.size() &&
                    n->path == path.substr(0, n->path.size()) &&
                    (n->path.size() >= path.size() ||
                     path[n->path.size()] == '/'))
                {
                    continue; // Recurse
                }
                else
                {
                    throw std::runtime_error("Conflict: " + fullPath +
                                             " with wildcard route.");
                }
            }

            // Check if we need to continue with a child node based on the first
            // character
            if (n->type == Param && path[0] == '/' && n->children.size() == 1)
            {
                n = n->children[0];
                continue;
            }

            // Traverse the children by matching indices
            bool matched = false;
            for (size_t i = 0; i < n->indices.size(); ++i)
            {
                if (path[0] == n->indices[i])
                {
                    n = n->children[i];
                    matched = true;
                    break;
                }
            }

            // Case 4: Create new child node if no match found
            if (!matched)
            {
                n->indices += path[0];
                node *child = new node();
                n->children.push_back(child);
                n = child;
            }
            n->insertChild(path, fullPath, handler);
            return;
        }

        // Case 5: No more segments, assign handler
        if (n->handler.size() > 0)
        {
            throw std::runtime_error("Conflict: " + fullPath +
                                     " with existing route.");
        }
        n->handler = handler;
        return;
    }
}

void node::insertChild(std::string path, std::string fullPath,
                       HandlerChain handler)
{
    node *n = this;
    while (true)
    {
        int pos;
        std::string wildcard;
        bool valid = findWildcard(path, wildcard, pos);
        if (pos < 0)
        {
            break;
        }

        if (!valid)
        {
            throw std::runtime_error("Conflict: " + path + " with " + fullPath);
        }

        if (wildcard.size() < 2)
        {
            throw std::runtime_error("Wildcards must have a non-empty name.");
        }

        if (!n->children.empty())
        {
            throw std::runtime_error(
                "Wildcard route conflicts with existing children in path '" +
                n->path + "'");
        }

        // Handle the wildcard param
        if (wildcard[0] == ':')
        {
            if (pos > 0)
            {
                n->path = path.substr(0, pos);
                path = path.substr(pos);
            }

            n->wildChild = true;
            node *child = new node{wildcard, "", Param, true, {}, {}};
            n->children = {child};
            n = child;

            if (wildcard.size() < path.size())
            {
                path = path.substr(wildcard.size());
                node *child = new node();
                n->children = {child};
                n = child;
                continue;
            }

            n->handler = handler;
            return;
        }
    }

    // No wildcard found, set path and handler
    n->path = path;
    n->handler = handler;
}

void Context::next()
{
    index++;
    for (; index < handlerChain.size(); index++)
    {
        if (handlerChain[index])
        {
            handlerChain[index](this);
        }
    }
}

void Context::abort() { index = handlerChain.size() + 10; }

auto Context::getParam(const std::string &key, std::string &param) -> bool
{
    if (params.find(key) != params.end())
    {
        param = params[key];
        return true;
    }
    return false;
}
