/**
 * @file ClassNameStrategy.h
 * @brief 类名混淆策略头文件
 * 
 * 定义ClassNameObfuscationStrategy类，用于混淆Objective-C类名。
 */

#ifndef CLASS_NAME_STRATEGY_H
#define CLASS_NAME_STRATEGY_H

#include "strategies/ObfuscationStrategy.h"
// 注意：由于ClassNameStrategy.h中使用了MatchFinder的完整类型（作为成员变量），
// 必须包含完整的头文件，不能使用前向声明
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include <map>
#include <vector>

namespace obfuscator {

/**
 * @class ClassNameObfuscationStrategy
 * @brief 类名混淆策略
 * 
 * 该策略负责混淆Objective-C类名，包括：
 * - @interface声明中的类名
 * - @implementation声明中的类名
 * - 所有使用该类名的地方
 * 
 * 继承自ObfuscationStrategy和MatchFinder::MatchCallback，
 * 使用AST Matcher来识别需要混淆的类。
 */
class ClassNameObfuscationStrategy : public ObfuscationStrategy,
                                     public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    /**
     * @brief 获取策略名称
     * @return 策略名称字符串
     */
    std::string getName() const override {
        return "ClassNameObfuscation";
    }
    
    /**
     * @brief 获取策略描述
     * @return 策略描述字符串
     */
    std::string getDescription() const override {
        return "混淆Objective-C类名";
    }
    
    /**
     * @brief 分析阶段：扫描AST，识别需要混淆的类
     * @param context AST上下文
     */
    void analyze(clang::ASTContext& context) override;
    
    /**
     * @brief 转换阶段：执行代码替换
     * @param context AST上下文
     * @param rewriter 代码重写器
     */
    void transform(clang::ASTContext& context, clang::Rewriter& rewriter) override;
    
    /**
     * @brief 验证阶段：检查混淆结果
     * @param context AST上下文
     * @return 验证通过返回true
     */
    bool validate(clang::ASTContext& context) const override;
    
    /**
     * @brief MatchFinder回调：处理匹配到的AST节点
     * @param result 匹配结果
     */
    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override;

private:
    /**
     * @brief 处理接口声明
     * @param decl ObjC接口声明节点
     */
    void handleInterface(const clang::ObjCInterfaceDecl* decl);
    
    /**
     * @brief 处理实现声明
     * @param decl ObjC实现声明节点
     */
    void handleImplementation(const clang::ObjCImplementationDecl* decl);
    
    /**
     * @brief 替换源代码中的类名
     * @param context AST上下文
     * @param rewriter 代码重写器
     * @param original 原始类名
     * @param obfuscated 混淆后的类名
     */
    void replaceClassName(clang::ASTContext& context,
                         clang::Rewriter& rewriter,
                         const std::string& original,
                         const std::string& obfuscated);
    
    /**
     * @brief 更新import语句
     * @param rewriter 代码重写器
     * @param original 原始类名
     * @param obfuscated 混淆后的类名
     */
    void updateImports(clang::Rewriter& rewriter,
                      const std::string& original,
                      const std::string& obfuscated);
    
    /**
     * @brief 获取类名映射表（用于文件重命名）
     * @return 类名到混淆名的映射表
     */
    const std::map<std::string, std::string>& getClassMapping() const {
        return classesToObfuscate_;
    }
    
    clang::ast_matchers::MatchFinder finder_;  ///< AST匹配器
    std::map<std::string, std::string> classesToObfuscate_;  ///< 需要混淆的类名映射表
};

} // namespace obfuscator

#endif // CLASS_NAME_STRATEGY_H

