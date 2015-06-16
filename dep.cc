#include "dep.h"

#include <memory>

#include "log.h"
#include "rule.h"
#include "var.h"

static vector<DepNode*>* g_dep_node_pool;

DepNode::DepNode(StringPiece o, bool p)
    : output(o),
      has_rule(false),
      is_order_only(false),
      is_phony(p),
      target_specific_vars(NULL) {
  g_dep_node_pool->push_back(this);
}

class DepBuilder {
 public:
  DepBuilder(const vector<Rule*>& rules,
             const Vars& vars,
             const unordered_map<StringPiece, Vars*>& rule_vars)
      : vars_(vars),
        rule_vars_(rule_vars),
        first_rule_(NULL) {
    PopulateRules(rules);
  }

  void Build(vector<StringPiece> targets,
             vector<DepNode*>* nodes) {
    if (targets.empty()) {
      if (!first_rule_) {
        ERROR("*** No targets.");
      }
      CHECK(!first_rule_->outputs.empty());
      targets.push_back(first_rule_->outputs[0]);
    }

    // TODO: LogStats?

    for (StringPiece target : targets) {
      unique_ptr<Vars> tsvs(new Vars);
      DepNode* n = BuildPlan(target, "", tsvs.get());
      nodes->push_back(n);
    }
  }

 private:
  void PopulateRules(const vector<Rule*>& rules) {
    for (Rule* rule : rules) {
      if (rule->outputs.empty()) {
        PopulateImplicitRule(rule);
      } else {
        PopulateExplicitRule(rule);
      }
    }
  }

  void PopulateExplicitRule(Rule* rule) {
    for (StringPiece output : rule->outputs) {
      // isSuffixRule := db.populateSuffixRule(rule, output)


      /*
          if oldRule, present := db.rules[output]; present {
     r := mergeRules(oldRule, rule, output, isSuffixRule)
                                                         db.rules[output] = r
                                                         } else {
        db.rules[output] = rule
            if db.firstRule == nil && !strings.HasPrefix(output, ".") {
                db.firstRule = rule
              }
      }
      */

      auto p = rules_.insert(make_pair(output, rule));
      if (p.second) {
        if (!first_rule_ && output.get(0) != '.') {
          first_rule_ = rule;
        }
      } else {
        // TODO: merge
        CHECK(false);
      }
    }
  }

  void PopulateImplicitRule(Rule*) {
    CHECK(false);
  }

  Rule* LookupRule(StringPiece o) {
    auto found = rules_.find(o);
    if (found != rules_.end())
      return found->second;
    return NULL;
  }

  Vars* LookupRuleVars(StringPiece o) {
    auto found = rule_vars_.find(o);
    if (found != rule_vars_.end())
      return found->second;
    return NULL;
  }

  bool PickRule(StringPiece output, Rule** r, Vars** v) {
    Rule* rule = LookupRule(output);
    Vars* vars = LookupRuleVars(output);
    *r = rule;
    *v = vars;
    if (rule) {
      if (!rule->cmds.empty()) {
        return true;
      }
    }
    return rule;
  }

  DepNode* BuildPlan(StringPiece output, StringPiece needed_by, Vars* tsvs) {
    LOG("BuildPlan: %s for %s",
        output.as_string().c_str(),
        needed_by.as_string().c_str());

    auto found = done_.find(output);
    if (found != done_.end()) {
      return found->second;
    }

    DepNode* n = new DepNode(output, phony_[output]);
    done_[output] = n;

    Rule* rule;
    Vars* vars;
    if (!PickRule(output, &rule, &vars)) {
      return n;
    }

    // TODO: Handle TSVs

    for (StringPiece input : rule->inputs) {
      if (rule->output_patterns.size() > 0) {
        if (rule->output_patterns.size() > 1) {
          ERROR("TODO: multiple output pattern is not supported yet");
        }
        ERROR("TODO");
      }

      n->actual_inputs.push_back(input);
      DepNode* c = BuildPlan(input, output, tsvs);
      n->deps.push_back(c);
    }

    // TODO: order only
    n->has_rule = true;
    n->cmds = rule->cmds;

    return n;
  }

  unordered_map<StringPiece, Rule*> rules_;
  const Vars& vars_;
  const unordered_map<StringPiece, Vars*>& rule_vars_;

  vector<Rule*> implicit_rules_;   // pattern=%. no prefix,suffix.
  //vector<Rule*> iprefix_rules_;   // pattern=prefix%..  may have suffix
  //vector<Rule*> isuffix_rules_;   // pattern=%suffix  no prefix

  unordered_map<StringPiece, vector<Rule*>> suffix_rules_;
  Rule* first_rule_;
  unordered_map<StringPiece, DepNode*> done_;
  unordered_map<StringPiece, bool> phony_;
};

void MakeDep(const vector<Rule*>& rules,
             const Vars& vars,
             const unordered_map<StringPiece, Vars*>& rule_vars,
             const vector<StringPiece>& targets,
             vector<DepNode*>* nodes) {
  DepBuilder db(rules, vars, rule_vars);
  db.Build(targets, nodes);
}

void InitDepNodePool() {
  g_dep_node_pool = new vector<DepNode*>;
}

void QuitDepNodePool() {
  for (DepNode* n : *g_dep_node_pool)
    delete n;
  delete g_dep_node_pool;
}