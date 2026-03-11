//
//  Hpub_YYKeychain.m
//  YYKit <https://github.com/ibireme/YYKit>
//
//  Created by ibireme on 14/10/15.
//  Copyright (c) 2015 ibireme.
//
//  This source code is licensed under the MIT-style license found in the
//  LICENSE file in the root directory of this source tree.
//

#import "Hpub_YYKeychain.h"

#import <Security/Security.h>
#import <UIKit/UIKit.h>


static Hpub_YYKeychainErrorCode Hpub_YYKeychainErrorCodeFromOSStatus(OSStatus status) {
    switch (status) {
        case errSecUnimplemented: return Hpub_YYKeychainErrorUnimplemented;
        case errSecIO: return Hpub_YYKeychainErrorIO;
        case errSecOpWr: return Hpub_YYKeychainErrorOpWr;
        case errSecParam: return Hpub_YYKeychainErrorParam;
        case errSecAllocate: return Hpub_YYKeychainErrorAllocate;
        case errSecUserCanceled: return Hpub_YYKeychainErrorUserCancelled;
        case errSecBadReq: return Hpub_YYKeychainErrorBadReq;
        case errSecInternalComponent: return Hpub_YYKeychainErrorInternalComponent;
        case errSecNotAvailable: return Hpub_YYKeychainErrorNotAvailable;
        case errSecDuplicateItem: return Hpub_YYKeychainErrorDuplicateItem;
        case errSecItemNotFound: return Hpub_YYKeychainErrorItemNotFound;
        case errSecInteractionNotAllowed: return Hpub_YYKeychainErrorInteractionNotAllowed;
        case errSecDecode: return Hpub_YYKeychainErrorDecode;
        case errSecAuthFailed: return Hpub_YYKeychainErrorAuthFailed;
        default: return 0;
    }
}

static NSString *Hpub_YYKeychainErrorDesc(Hpub_YYKeychainErrorCode code) {
    switch (code) {
        case Hpub_YYKeychainErrorUnimplemented:
            return @"Function or operation not implemented.";
        case Hpub_YYKeychainErrorIO:
            return @"I/O error (bummers)";
        case Hpub_YYKeychainErrorOpWr:
            return @"ile already open with with write permission.";
        case Hpub_YYKeychainErrorParam:
            return @"One or more parameters passed to a function where not valid.";
        case Hpub_YYKeychainErrorAllocate:
            return @"Failed to allocate memory.";
        case Hpub_YYKeychainErrorUserCancelled:
            return @"User canceled the operation.";
        case Hpub_YYKeychainErrorBadReq:
            return @"Bad parameter or invalid state for operation.";
        case Hpub_YYKeychainErrorInternalComponent:
            return @"Inrernal Component";
        case Hpub_YYKeychainErrorNotAvailable:
            return @"No keychain is available. You may need to restart your computer.";
        case Hpub_YYKeychainErrorDuplicateItem:
            return @"The specified item already exists in the keychain.";
        case Hpub_YYKeychainErrorItemNotFound:
            return @"The specified item could not be found in the keychain.";
        case Hpub_YYKeychainErrorInteractionNotAllowed:
            return @"User interaction is not allowed.";
        case Hpub_YYKeychainErrorDecode:
            return @"Unable to decode the provided data.";
        case Hpub_YYKeychainErrorAuthFailed:
            return @"The user name or passphrase you entered is not";
        default:
            break;
    }
    return nil;
}

static NSString *Hpub_YYKeychainAccessibleStr(Hpub_YYKeychainAccessible e) {
    switch (e) {
        case Hpub_YYKeychainAccessibleWhenUnlocked:
            return (__bridge NSString *)(kSecAttrAccessibleWhenUnlocked);
        case Hpub_YYKeychainAccessibleAfterFirstUnlock:
            return (__bridge NSString *)(kSecAttrAccessibleAfterFirstUnlock);
        case Hpub_YYKeychainAccessibleAlways:
            return (__bridge NSString *)(kSecAttrAccessibleAlways);
        case Hpub_YYKeychainAccessibleWhenPasscodeSetThisDeviceOnly:
            return (__bridge NSString *)(kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly);
        case Hpub_YYKeychainAccessibleWhenUnlockedThisDeviceOnly:
            return (__bridge NSString *)(kSecAttrAccessibleWhenUnlockedThisDeviceOnly);
        case Hpub_YYKeychainAccessibleAfterFirstUnlockThisDeviceOnly:
            return (__bridge NSString *)(kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly);
        case Hpub_YYKeychainAccessibleAlwaysThisDeviceOnly:
            return (__bridge NSString *)(kSecAttrAccessibleAlwaysThisDeviceOnly);
        default:
            return nil;
    }
}

static Hpub_YYKeychainAccessible Hpub_YYKeychainAccessibleEnum(NSString *s) {
    if ([s isEqualToString:(__bridge NSString *)kSecAttrAccessibleWhenUnlocked])
        return Hpub_YYKeychainAccessibleWhenUnlocked;
    if ([s isEqualToString:(__bridge NSString *)kSecAttrAccessibleAfterFirstUnlock])
        return Hpub_YYKeychainAccessibleAfterFirstUnlock;
    if ([s isEqualToString:(__bridge NSString *)kSecAttrAccessibleAlways])
        return Hpub_YYKeychainAccessibleAlways;
    if ([s isEqualToString:(__bridge NSString *)kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly])
        return Hpub_YYKeychainAccessibleWhenPasscodeSetThisDeviceOnly;
    if ([s isEqualToString:(__bridge NSString *)kSecAttrAccessibleWhenUnlockedThisDeviceOnly])
        return Hpub_YYKeychainAccessibleWhenUnlockedThisDeviceOnly;
    if ([s isEqualToString:(__bridge NSString *)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly])
        return Hpub_YYKeychainAccessibleAfterFirstUnlockThisDeviceOnly;
    if ([s isEqualToString:(__bridge NSString *)kSecAttrAccessibleAlwaysThisDeviceOnly])
        return Hpub_YYKeychainAccessibleAlwaysThisDeviceOnly;
    return Hpub_YYKeychainAccessibleNone;
}

static id Hpub_YYKeychainQuerySynchonizationID(Hpub_YYKeychainQuerySynchronizationMode mode) {
    switch (mode) {
        case Hpub_YYKeychainQuerySynchronizationModeAny:
            return (__bridge id)(kSecAttrSynchronizableAny);
        case Hpub_YYKeychainQuerySynchronizationModeNo:
            return (__bridge id)kCFBooleanFalse;
        case Hpub_YYKeychainQuerySynchronizationModeYes:
            return (__bridge id)kCFBooleanTrue;
        default:
            return (__bridge id)(kSecAttrSynchronizableAny);
    }
}

static Hpub_YYKeychainQuerySynchronizationMode Hpub_YYKeychainQuerySynchonizationEnum(NSNumber *num) {
    if ([num isEqualToNumber:@NO]) return Hpub_YYKeychainQuerySynchronizationModeNo;
    if ([num isEqualToNumber:@YES]) return Hpub_YYKeychainQuerySynchronizationModeYes;
    return Hpub_YYKeychainQuerySynchronizationModeAny;
}

@interface Hpub_YYKeychainItem ()
@property (nonatomic, readwrite, strong) NSDate *modificationDate;
@property (nonatomic, readwrite, strong) NSDate *creationDate;
@end

@implementation Hpub_YYKeychainItem


- (void)setPasswordObject:(id <NSCoding> )object {
    self.passwordData = [NSKeyedArchiver archivedDataWithRootObject:object];
}

- (id <NSCoding> )passwordObject {
    if ([self.passwordData length]) {
        return [NSKeyedUnarchiver unarchiveObjectWithData:self.passwordData];
    }
    return nil;
}

- (void)setPassword:(NSString *)password {
    self.passwordData = [password dataUsingEncoding:NSUTF8StringEncoding];
}

- (NSString *)password {
    if ([self.passwordData length]) {
        return [[NSString alloc] initWithData:self.passwordData encoding:NSUTF8StringEncoding];
    }
    return nil;
}

- (NSMutableDictionary *)queryDic {
    NSMutableDictionary *dic = [NSMutableDictionary new];
    
    dic[(__bridge id)kSecClass] = (__bridge id)kSecClassGenericPassword;
    
    if (self.account) dic[(__bridge id)kSecAttrAccount] = self.account;
    if (self.service) dic[(__bridge id)kSecAttrService] = self.service;
    
    if (!self.isSimulator) {
        // Remove the access group if running on the iPhone simulator.
        //
        // Apps that are built for the simulator aren't signed, so there's no keychain access group
        // for the simulator to check. This means that all apps can see all keychain items when run
        // on the simulator.
        //
        // If a SecItem contains an access group attribute, SecItemAdd and SecItemUpdate on the
        // simulator will return -25243 (errSecNoAccessForItem).
        //
        // The access group attribute will be included in items returned by SecItemCopyMatching,
        // which is why we need to remove it before updating the item.
        if (self.accessGroup) dic[(__bridge id)kSecAttrAccessGroup] = self.accessGroup;
    }
    
    if ([UIDevice currentDevice].systemVersion.doubleValue >= 7) {
        dic[(__bridge id)kSecAttrSynchronizable] = Hpub_YYKeychainQuerySynchonizationID(self.synchronizable);
    }
    
    return dic;
}

- (NSMutableDictionary *)dic {
    NSMutableDictionary *dic = [NSMutableDictionary new];
    
    dic[(__bridge id)kSecClass] = (__bridge id)kSecClassGenericPassword;
    
    if (self.account) dic[(__bridge id)kSecAttrAccount] = self.account;
    if (self.service) dic[(__bridge id)kSecAttrService] = self.service;
    if (self.label) dic[(__bridge id)kSecAttrLabel] = self.label;
    
    if (!self.isSimulator) {
        // Remove the access group if running on the iPhone simulator.
        //
        // Apps that are built for the simulator aren't signed, so there's no keychain access group
        // for the simulator to check. This means that all apps can see all keychain items when run
        // on the simulator.
        //
        // If a SecItem contains an access group attribute, SecItemAdd and SecItemUpdate on the
        // simulator will return -25243 (errSecNoAccessForItem).
        //
        // The access group attribute will be included in items returned by SecItemCopyMatching,
        // which is why we need to remove it before updating the item.
        if (self.accessGroup) dic[(__bridge id)kSecAttrAccessGroup] = self.accessGroup;
    }
    
    if ([UIDevice currentDevice].systemVersion.doubleValue >= 7) {
        dic[(__bridge id)kSecAttrSynchronizable] = Hpub_YYKeychainQuerySynchonizationID(self.synchronizable);
    }
    
    if (self.accessible) dic[(__bridge id)kSecAttrAccessible] = Hpub_YYKeychainAccessibleStr(self.accessible);
    if (self.passwordData) dic[(__bridge id)kSecValueData] = self.passwordData;
    if (self.type) dic[(__bridge id)kSecAttrType] = self.type;
    if (self.creater) dic[(__bridge id)kSecAttrCreator] = self.creater;
    if (self.comment) dic[(__bridge id)kSecAttrComment] = self.comment;
    if (self.descr) dic[(__bridge id)kSecAttrDescription] = self.descr;
    
    return dic;
}


- (BOOL)isSimulator {
    static dispatch_once_t one;
    static BOOL simu;
    dispatch_once(&one, ^{
        simu = NSNotFound != [[UIDevice currentDevice].model rangeOfString:@"Simulator"].location;
    });
    return simu;
}

- (instancetype)initWithDic:(NSDictionary *)dic {
    if (dic.count == 0) return nil;
    self = self.init;
    
    self.service = dic[(__bridge id)kSecAttrService];
    self.account = dic[(__bridge id)kSecAttrAccount];
    self.passwordData = dic[(__bridge id)kSecValueData];
    self.label = dic[(__bridge id)kSecAttrLabel];
    self.type = dic[(__bridge id)kSecAttrType];
    self.creater = dic[(__bridge id)kSecAttrCreator];
    self.comment = dic[(__bridge id)kSecAttrComment];
    self.descr = dic[(__bridge id)kSecAttrDescription];
    self.modificationDate = dic[(__bridge id)kSecAttrModificationDate];
    self.creationDate = dic[(__bridge id)kSecAttrCreationDate];
    self.accessGroup = dic[(__bridge id)kSecAttrAccessGroup];
    self.accessible = Hpub_YYKeychainAccessibleEnum(dic[(__bridge id)kSecAttrAccessible]);
    self.synchronizable = Hpub_YYKeychainQuerySynchonizationEnum(dic[(__bridge id)kSecAttrSynchronizable]);
    
    return self;
}





- (id)copyWithZone:(NSZone *)zone {
    Hpub_YYKeychainItem *item = [Hpub_YYKeychainItem new];
    item.service = self.service;
    item.account = self.account;
    item.passwordData = self.passwordData;
    item.label = self.label;
    item.type = self.type;
    item.creater = self.creater;
    item.comment = self.comment;
    item.descr = self.descr;
    item.modificationDate = self.modificationDate;
    item.creationDate = self.creationDate;
    item.accessGroup = self.accessGroup;
    item.accessible = self.accessible;
    item.synchronizable = self.synchronizable;
    return item;
}

- (NSString *)description {
    NSMutableString *str = @"".mutableCopy;
    [str appendString:@"Hpub_YYKeychainItem:{\n"];
    if (self.service) [str appendFormat:@"  service:%@,\n", self.service];
    if (self.account) [str appendFormat:@"  service:%@,\n", self.account];
    if (self.password) [str appendFormat:@"  service:%@,\n", self.password];
    if (self.label) [str appendFormat:@"  service:%@,\n", self.label];
    if (self.type) [str appendFormat:@"  service:%@,\n", self.type];
    if (self.creater) [str appendFormat:@"  service:%@,\n", self.creater];
    if (self.comment) [str appendFormat:@"  service:%@,\n", self.comment];
    if (self.descr) [str appendFormat:@"  service:%@,\n", self.descr];
    if (self.modificationDate) [str appendFormat:@"  service:%@,\n", self.modificationDate];
    if (self.creationDate) [str appendFormat:@"  service:%@,\n", self.creationDate];
    if (self.accessGroup) [str appendFormat:@"  service:%@,\n", self.accessGroup];
    [str appendString:@"}"];
    return str;
}

@end



@implementation Hpub_YYKeychain

+ (NSString *)getPasswordForService:(NSString *)serviceName
                            account:(NSString *)account
                              error:(NSError **)error {
    if (!serviceName || !account) {
        if (error) *error = [Hpub_YYKeychain errorWithCode:errSecParam];
        return nil;
    }
    
    Hpub_YYKeychainItem *item = [Hpub_YYKeychainItem new];
    item.service = serviceName;
    item.account = account;
    Hpub_YYKeychainItem *result = [self selectOneItem:item error:error];
    return result.password;
}

+ (nullable NSString *)getPasswordForService:(NSString *)serviceName
                                     account:(NSString *)account {
    return [self getPasswordForService:serviceName account:account error:NULL];
}

+ (BOOL)deletePasswordForService:(NSString *)serviceName
                         account:(NSString *)account
                           error:(NSError **)error {
    if (!serviceName || !account) {
        if (error) *error = [Hpub_YYKeychain errorWithCode:errSecParam];
        return NO;
    }
    
    Hpub_YYKeychainItem *item = [Hpub_YYKeychainItem new];
    item.service = serviceName;
    item.account = account;
    return [self deleteItem:item error:error];
}

+ (BOOL)deletePasswordForService:(NSString *)serviceName account:(NSString *)account {
    return [self deletePasswordForService:serviceName account:account error:NULL];
}

+ (BOOL)setPassword:(NSString *)password
         forService:(NSString *)serviceName
            account:(NSString *)account
              error:(NSError **)error {
    if (!password || !serviceName || !account) {
        if (error) *error = [Hpub_YYKeychain errorWithCode:errSecParam];
        return NO;
    }
    Hpub_YYKeychainItem *item = [Hpub_YYKeychainItem new];
    item.service = serviceName;
    item.account = account;
    Hpub_YYKeychainItem *result = [self selectOneItem:item error:NULL];
    if (result) {
        result.password = password;
        return [self updateItem:result error:error];
    } else {
        item.password = password;
        return [self insertItem:item error:error];
    }
}

+ (BOOL)setPassword:(NSString *)password
         forService:(NSString *)serviceName
            account:(NSString *)account {
    return [self setPassword:password forService:serviceName account:account error:NULL];
}

+ (BOOL)insertItem:(Hpub_YYKeychainItem *)item error:(NSError **)error {
    if (!item.service || !item.account || !item.passwordData) {
        if (error) *error = [Hpub_YYKeychain errorWithCode:errSecParam];
        return NO;
    }
    
    NSMutableDictionary *query = [item dic];
    OSStatus status = status = SecItemAdd((__bridge CFDictionaryRef)query, NULL);
    if (status != errSecSuccess) {
        if (error) *error = [Hpub_YYKeychain errorWithCode:status];
        return NO;
    }
    
    return YES;
}

+ (BOOL)insertItem:(Hpub_YYKeychainItem *)item {
    return [self insertItem:item error:NULL];
}

+ (BOOL)updateItem:(Hpub_YYKeychainItem *)item error:(NSError **)error {
    if (!item.service || !item.account || !item.passwordData) {
        if (error) *error = [Hpub_YYKeychain errorWithCode:errSecParam];
        return NO;
    }
    
    NSMutableDictionary *query = [item queryDic];
    NSMutableDictionary *update = [item dic];
    [update removeObjectForKey:(__bridge id)kSecClass];
    if (!query || !update) return NO;
    OSStatus status = status = SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)update);
    if (status != errSecSuccess) {
        if (error) *error = [Hpub_YYKeychain errorWithCode:status];
        return NO;
    }
    
    return YES;
}

+ (BOOL)updateItem:(Hpub_YYKeychainItem *)item {
    return [self updateItem:item error:NULL];
}

+ (BOOL)deleteItem:(Hpub_YYKeychainItem *)item error:(NSError **)error {
    if (!item.service || !item.account) {
        if (error) *error = [Hpub_YYKeychain errorWithCode:errSecParam];
        return NO;
    }
    
    NSMutableDictionary *query = [item dic];
    OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);
    if (status != errSecSuccess) {
        if (error) *error = [Hpub_YYKeychain errorWithCode:status];
        return NO;
    }
    
    return YES;
}

+ (BOOL)deleteItem:(Hpub_YYKeychainItem *)item {
    return [self deleteItem:item error:NULL];
}

+ (Hpub_YYKeychainItem *)selectOneItem:(Hpub_YYKeychainItem *)item error:(NSError **)error {
    if (!item.service || !item.account) {
        if (error) *error = [Hpub_YYKeychain errorWithCode:errSecParam];
        return nil;
    }
    
    NSMutableDictionary *query = [item dic];
    query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;
    query[(__bridge id)kSecReturnAttributes] = @YES;
    query[(__bridge id)kSecReturnData] = @YES;
    
    OSStatus status;
    CFTypeRef result = NULL;
    status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
    if (status != errSecSuccess) {
        if (error) *error = [[self class] errorWithCode:status];
        return nil;
    }
    if (!result) return nil;
    
    NSDictionary *dic = nil;
    if (CFGetTypeID(result) == CFDictionaryGetTypeID()) {
        dic = (__bridge NSDictionary *)(result);
    } else if (CFGetTypeID(result) == CFArrayGetTypeID()){
        dic = [(__bridge NSArray *)(result) firstObject];
        if (![dic isKindOfClass:[NSDictionary class]]) dic = nil;
    }
    if (!dic.count) return nil;
    
#pragma mark - 修复内存泄露问题 bycaf
    Hpub_YYKeychainItem *kitem = [[Hpub_YYKeychainItem alloc] initWithDic:dic];
    if(status == errSecSuccess){
        
        // ...
        CFRelease(result);
    }
    
    return kitem;
}

+ (Hpub_YYKeychainItem *)selectOneItem:(Hpub_YYKeychainItem *)item {
    return [self selectOneItem:item error:NULL];
}

+ (NSArray *)selectItems:(Hpub_YYKeychainItem *)item error:(NSError **)error {
    NSMutableDictionary *query = [item dic];
    query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitAll;
    query[(__bridge id)kSecReturnAttributes] = @YES;
    query[(__bridge id)kSecReturnData] = @YES;
    
    OSStatus status;
    CFTypeRef result = NULL;
    status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
    if (status != errSecSuccess && error != NULL) {
        *error = [[self class] errorWithCode:status];
        return nil;
    }
    
    NSMutableArray *res = [NSMutableArray new];
    NSDictionary *dic = nil;
    if (CFGetTypeID(result) == CFDictionaryGetTypeID()) {
        dic = (__bridge NSDictionary *)(result);
        Hpub_YYKeychainItem *item = [[Hpub_YYKeychainItem alloc] initWithDic:dic];
        if (item) [res addObject:item];
    } else if (CFGetTypeID(result) == CFArrayGetTypeID()){
        for (NSDictionary *dic in (__bridge NSArray *)(result)) {
            Hpub_YYKeychainItem *item = [[Hpub_YYKeychainItem alloc] initWithDic:dic];
            if (item) [res addObject:item];
        }
    }
    
    return res;
}

+ (NSArray *)selectItems:(Hpub_YYKeychainItem *)item {
    return [self selectItems:item error:NULL];
}

+ (NSError *)errorWithCode:(OSStatus)osCode {
    Hpub_YYKeychainErrorCode code = Hpub_YYKeychainErrorCodeFromOSStatus(osCode);
    NSString *desc = Hpub_YYKeychainErrorDesc(code);
    NSDictionary *userInfo = desc ? @{ NSLocalizedDescriptionKey : desc } : nil;
    return [NSError errorWithDomain:@"com.ibireme.yykit.keychain" code:code userInfo:userInfo];
}

@end
